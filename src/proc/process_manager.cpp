#include "proc/process_manager.h"
#include <limits>
#include <iostream>
#include <vector>
#include "proc/program.h"
#include "common/config.h"

namespace {
constexpr uint64_t kAutoScriptFd = std::numeric_limits<uint64_t>::max();
constexpr size_t kMaxScriptIoBytes = 1 << 20;  // 1 MiB safety cap
constexpr char kWriteFillByte = 'x';

// 把设备转交给“仍在等待”的进程，并唤醒它；无效/不匹配的 pid 会被跳过。
void wakeup_device_waiter(DeviceManager& device_manager,
                          std::map<int, PCB>& processes,
                          std::queue<int>& ready_queue,
                          uint32_t dev_id,
                          std::optional<int> next_owner_pid) {
    while (next_owner_pid) {
        const int pid = *next_owner_pid;
        const auto it = processes.find(pid);
        if (it == processes.end()) {
            next_owner_pid = device_manager.release(pid, dev_id);
            continue;
        }

        PCB& pcb = it->second;
        if (pcb.state == ProcessState::Blocked &&
            pcb.blocked_reason == BlockReason::Device &&
            pcb.waiting_device == dev_id) {
            pcb.state = ProcessState::Ready;
            pcb.blocked_time = 0;
            pcb.blocked_reason = BlockReason::None;
            pcb.waiting_device = UINT32_MAX;
            ready_queue.push(pid);
            std::cerr << "[Dev] Wakeup pid=" << pid << " for dev=" << dev_id
                      << "\n";
            return;
        }

        next_owner_pid = device_manager.release(pid, dev_id);
    }
}
}  // 命名空间

ProcessManager::ProcessManager(MemoryManager& memory_manager,
                               DeviceManager& device_manager,
                               FileSystem& file_system)
    : memory_manager_(memory_manager),
      device_manager_(device_manager),
      file_system_(file_system) {}

int ProcessManager::create_process(int total_time) {
    auto program = Program::create_default(total_time);
    return create_process_with_program(program);
}

int ProcessManager::create_process_from_file(const std::string& filename) {
    auto program = Program::load_from_file(filename);
    if (!program) {
        std::cerr << "Failed to load program from " << filename << std::endl;
        return -1;
    }
    return create_process_with_program(program);
}

int ProcessManager::create_process_with_program(
    std::shared_ptr<Program> program) {
    int pid = next_pid_++;
    PCB pcb;
    pcb.pid = pid;
    pcb.state = ProcessState::Ready;
    pcb.program = program;
    pcb.total_time = program->size();
    pcb.virtual_pages = config::DEFAULT_VIRTUAL_PAGES;

    processes_[pid] = pcb;
    ready_queue_.push(pid);
    
    // 为进程创建内存空间
    memory_manager_.create_process_memory(pid, pcb.virtual_pages);

    std::cerr << "Process " << pid << " created with " << program->size()
              << " instructions\n";
    return pid;
}

void ProcessManager::terminate_process(int pid) {
    auto it = processes_.find(pid);
    if (it == processes_.end()) {
        std::cerr << "Process " << pid << " not found.\n";
        return;
    }
    
    for (const auto& [dev_id, next_owner_pid] :
         device_manager_.release_all(pid)) {
        wakeup_device_waiter(device_manager_, processes_, ready_queue_, dev_id,
                             next_owner_pid);
    }

    close_all_process_files(it->second);

    // 释放进程的内存资源
    memory_manager_.free_process_memory(pid);
    
    processes_.erase(it);
    if (pid == cur_pid_) {
        cur_pid_ = -1;
    }
    std::cerr << "Process " << pid << " terminated.\n";
}

void ProcessManager::dump_processes() const {
    std::cerr << "PID\tState\t\tRemain\tCPU/Total\tBlocked\n";
    for (const auto& [pid, pcb] : processes_) {
        std::string state_str;
        switch (pcb.state) {
            case ProcessState::New:
                state_str = "New";
                break;
            case ProcessState::Ready:
                state_str = "Ready";
                break;
            case ProcessState::Running:
                state_str = "Running";
                break;
            case ProcessState::Blocked:
                state_str = "Blocked";
                break;
            case ProcessState::Terminated:
                state_str = "Terminated";
                break;
        }
        std::cerr << pid << "\t" << state_str << "\t\t" << pcb.time_slice_left
                  << "\t" << pcb.cpu_time << "/" << pcb.total_time << "\t\t"
                  << pcb.blocked_time << "\n";
    }
    if (cur_pid_ != -1) {
        std::cerr << "Currently running: " << cur_pid_ << "\n";
    } else {
        std::cerr << "CPU idle\n";
    }
}

void ProcessManager::tick() {
    std::cerr << "=== Tick " << next_tick_++ << " === (Total: " << processes_.size();
    if (cur_pid_ != -1) {
        std::cerr << " | Running: PID=" << cur_pid_ << " PC=" << processes_[cur_pid_].pc;
    } else {
        std::cerr << " | CPU Idle";
    }
    std::cerr << ")\n";

    if (cur_pid_ == -1) {
        schedule();
    }

    if (cur_pid_ != -1) {
        if (processes_.find(cur_pid_) == processes_.end()) {
            throw std::runtime_error("Current PID not found in process list");
        }
        auto& pcb = processes_[cur_pid_];
        // 执行下一条指令
        if (pcb.pc < pcb.program->size()) {
            execute_instruction(pcb, pcb.program->get_instruction(pcb.pc));
            pcb.pc++;
        }

        pcb.time_slice_left--;
        pcb.cpu_time++;
        std::cerr << "[Tick] Process " << cur_pid_
                  << " executing (PC=" << pcb.pc << "/" << pcb.program->size()
                  << ", slice remaining: " << pcb.time_slice_left << ")\n";

        if (pcb.pc >= pcb.program->size()) {  // 进程完成
            std::cerr << "[Tick] Process " << cur_pid_ << " completed\n";
            pcb.state = ProcessState::Terminated;
            for (const auto& [dev_id, next_owner_pid] :
                 device_manager_.release_all(cur_pid_)) {
                wakeup_device_waiter(device_manager_, processes_, ready_queue_,
                                     dev_id, next_owner_pid);
            }
            close_all_process_files(pcb);
            memory_manager_.free_process_memory(cur_pid_);
            processes_.erase(cur_pid_);
            cur_pid_ = -1;
        } else if (pcb.time_slice_left <= 0) {  // 时间片完
            std::cerr << "[Tick] Process " << cur_pid_
                      << " time slice exhausted\n";
            pcb.state = ProcessState::Ready;
            pcb.time_slice_left = pcb.time_slice;
            ready_queue_.push(cur_pid_);
            cur_pid_ = -1;
        } else if (pcb.state == ProcessState::Blocked) {  // 进程阻塞
            std::cerr << "[Tick] Process " << cur_pid_
                      << " blocked during execution\n";
            cur_pid_ = -1;
        }
    }

    check_blocked_processes();
}

void ProcessManager::schedule() {
    while (ready_queue_.size()) {
        int pid = ready_queue_.front();
        ready_queue_.pop();
        if (processes_.find(pid) == processes_.end()) {
            continue;  // 就绪队列可能存在已被终止的非法进程
        }
        auto& pcb = processes_[pid];
        if (pcb.state != ProcessState::Ready) {
            continue;  // 进程未就绪
        }
        // 调度进程开始运行
        cur_pid_ = pcb.pid;
        pcb.state = ProcessState::Running;
        std::cerr << "[Schedule] Process " << pid << " is now running\n";
        return;
    }
    // 未能调度任何进程
    std::cerr << "[Schedule] CPU idle - no ready processes\n";
}

void ProcessManager::run_process(int pid) {
    if (processes_.find(pid) == processes_.end()) {
        std::cerr << "Process " << pid << " not found.\n";
        return;
    }
    if (processes_[pid].state != ProcessState::Ready) {
        std::cerr << "Process " << pid << " is not in Ready state\n";
        return;
    }

    if (cur_pid_ != -1) {  // 抢占，改变当前进程状态
        processes_[cur_pid_].state = ProcessState::Ready;
        ready_queue_.push(cur_pid_);
        std::cerr << "Process " << cur_pid_ << " preempted\n";
    }

    auto& pcb = processes_[pid];
    cur_pid_ = pid;
    pcb.state = ProcessState::Running;
    std::cerr << "Process " << pid << " is now running\n";
}

void ProcessManager::block_process(int pid, int duration) {
    if (processes_.find(pid) == processes_.end()) {
        std::cerr << "Process " << pid << " not found.\n";
        return;
    }
    auto& pcb = processes_[pid];
    if (pcb.state != ProcessState::Running &&
        pcb.state != ProcessState::Ready) {
        std::cerr << "Process " << pid
                  << " cannot be blocked in its current state\n";
        return;
    }

    pcb.state = ProcessState::Blocked;
    pcb.blocked_time = duration;
    pcb.blocked_reason = BlockReason::Sleep;
    pcb.waiting_device = UINT32_MAX;
    std::cerr << "Process " << pid << " is blocked for " << duration
              << " ticks\n";

    if (pid == cur_pid_) {  // 当前进程被阻塞，触发调度
        cur_pid_ = -1;
        schedule();
    }
    // 就绪队列中可能存在该进程的冗余项，暂不移除
}

void ProcessManager::wakeup_process(int pid) {
    if (processes_.find(pid) == processes_.end()) {
        std::cerr << "Process " << pid << " not found.\n";
        return;
    }
    auto& pcb = processes_[pid];
    if (pcb.state != ProcessState::Blocked) {
        std::cerr << "Process " << pid << " is not blocked\n";
        return;
    }

    pcb.state = ProcessState::Ready;
    pcb.blocked_time = 0;
    pcb.blocked_reason = BlockReason::None;
    pcb.waiting_device = UINT32_MAX;
    device_manager_.cancel_wait(pid);
    ready_queue_.push(pid);
    std::cerr << "Process " << pid << " woken up and added to ready queue\n";
}

void ProcessManager::check_blocked_processes() {
    // 更新阻塞进程的阻塞时间
    for (auto& [pid, pcb] : processes_) {
        if (pcb.state == ProcessState::Blocked &&
            pcb.blocked_reason == BlockReason::Sleep && pcb.blocked_time > 0) {
            pcb.blocked_time--;
            if (pcb.blocked_time <= 0) {
                pcb.state = ProcessState::Ready;
                ready_queue_.push(pid);
                pcb.blocked_reason = BlockReason::None;
                std::cerr << "[Tick] Process " << pid << " auto-woken up\n";
            }
        }
    }
}

int ProcessManager::allocate_script_fd(PCB& pcb) {
    while (pcb.next_script_fd < std::numeric_limits<int>::max() &&
           pcb.fd_map.find(pcb.next_script_fd) != pcb.fd_map.end()) {
        ++pcb.next_script_fd;
    }
    if (pcb.next_script_fd >= std::numeric_limits<int>::max()) {
        return -1;
    }
    return pcb.next_script_fd++;
}

void ProcessManager::close_all_process_files(PCB& pcb) {
    for (const auto& [script_fd, fs_fd] : pcb.fd_map) {
        (void)script_fd;
        file_system_.close_file(fs_fd);
    }
    if (!pcb.fd_map.empty()) {
        std::cerr << "[Exec] Closed " << pcb.fd_map.size()
                  << " open file(s) for PID " << pcb.pid << "\n";
    }
    pcb.fd_map.clear();
}

void ProcessManager::execute_instruction(PCB& pcb, const Instruction& inst) {
    std::cerr << "[Exec] ";
    switch (inst.type) {
        case OpType::Compute:
            std::cerr << "Compute\n";
            break;
        case OpType::MemRead:
            std::cerr << "MemRead addr=" << inst.arg1 << "\n";
            memory_manager_.access_memory(pcb.pid, inst.arg1, AccessType::Read);
            break;
        case OpType::MemWrite:
            std::cerr << "MemWrite addr=" << inst.arg1 << "\n";
            memory_manager_.access_memory(pcb.pid, inst.arg1, AccessType::Write);
            break;
        case OpType::FileOpen: {
            int script_fd = -1;
            if (inst.arg1 != kAutoScriptFd) {
                if (inst.arg1 > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
                    std::cerr << "FileOpen invalid fd=" << inst.arg1 << "\n";
                    break;
                }
                script_fd = static_cast<int>(inst.arg1);
                if (script_fd < 3) {
                    std::cerr << "FileOpen invalid fd=" << script_fd << "\n";
                    break;
                }
                if (pcb.fd_map.find(script_fd) != pcb.fd_map.end()) {
                    std::cerr << "FileOpen fd already in use: " << script_fd
                              << "\n";
                    break;
                }
                if (script_fd >= pcb.next_script_fd) {
                    pcb.next_script_fd = script_fd + 1;
                }
            }

            int fs_fd = file_system_.open_file(inst.str_arg);
            if (fs_fd < 0) {
                std::cerr << "FileOpen failed: " << inst.str_arg << "\n";
                break;
            }

            if (inst.arg1 == kAutoScriptFd) {
                script_fd = allocate_script_fd(pcb);
                if (script_fd < 0) {
                    file_system_.close_file(fs_fd);
                    std::cerr << "FileOpen failed: no available script fd\n";
                    break;
                }
            }

            pcb.fd_map[script_fd] = fs_fd;
            std::cerr << "FileOpen file=" << inst.str_arg
                      << " -> fd=" << script_fd << "\n";
            break;
        }
        case OpType::FileClose: {
            if (inst.arg1 > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
                std::cerr << "FileClose invalid fd=" << inst.arg1 << "\n";
                break;
            }
            const int script_fd = static_cast<int>(inst.arg1);
            auto it = pcb.fd_map.find(script_fd);
            if (it == pcb.fd_map.end()) {
                std::cerr << "FileClose unknown fd=" << script_fd << "\n";
                break;
            }
            file_system_.close_file(it->second);
            pcb.fd_map.erase(it);
            std::cerr << "FileClose fd=" << script_fd << "\n";
            break;
        }
        case OpType::FileRead: {
            if (inst.arg1 > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
                std::cerr << "FileRead invalid fd=" << inst.arg1 << "\n";
                break;
            }
            const int script_fd = static_cast<int>(inst.arg1);
            auto it = pcb.fd_map.find(script_fd);
            if (it == pcb.fd_map.end()) {
                std::cerr << "FileRead unknown fd=" << script_fd << "\n";
                break;
            }

            size_t req = static_cast<size_t>(inst.arg2);
            if (req > kMaxScriptIoBytes) {
                req = kMaxScriptIoBytes;
            }
            std::vector<char> buf(req == 0 ? 1 : req);
            const ssize_t n = file_system_.read_file(it->second, buf.data(), req);
            if (n < 0) {
                std::cerr << "FileRead failed fd=" << script_fd
                          << " size=" << req << "\n";
            } else {
                std::cerr << "FileRead fd=" << script_fd << " size=" << req
                          << " -> " << n << " bytes\n";
            }
            break;
        }
        case OpType::FileWrite: {
            if (inst.arg1 > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
                std::cerr << "FileWrite invalid fd=" << inst.arg1 << "\n";
                break;
            }
            const int script_fd = static_cast<int>(inst.arg1);
            auto it = pcb.fd_map.find(script_fd);
            if (it == pcb.fd_map.end()) {
                std::cerr << "FileWrite unknown fd=" << script_fd << "\n";
                break;
            }

            size_t req = static_cast<size_t>(inst.arg2);
            if (req > kMaxScriptIoBytes) {
                req = kMaxScriptIoBytes;
            }
            std::vector<char> buf(req, kWriteFillByte);
            const ssize_t n = file_system_.write_file(it->second, buf.data(), req);
            if (n < 0) {
                std::cerr << "FileWrite failed fd=" << script_fd
                          << " size=" << req << "\n";
            } else {
                std::cerr << "FileWrite fd=" << script_fd << " size=" << req
                          << " -> " << n << " bytes\n";
            }
            break;
        }
        case OpType::DevRequest:
            std::cerr << "DevRequest dev=" << inst.arg1 << "\n";
            if (!device_manager_.request(pcb.pid,
                                         static_cast<uint32_t>(inst.arg1))) {
                pcb.state = ProcessState::Blocked;
                pcb.blocked_time = 0;
                pcb.blocked_reason = BlockReason::Device;
                pcb.waiting_device = static_cast<uint32_t>(inst.arg1);
            }
            break;
        case OpType::DevRelease:
            std::cerr << "DevRelease dev=" << inst.arg1 << "\n";
            wakeup_device_waiter(device_manager_, processes_, ready_queue_,
                                 static_cast<uint32_t>(inst.arg1),
                                 device_manager_.release(
                                     pcb.pid,
                                     static_cast<uint32_t>(inst.arg1)));
            break;
        case OpType::Sleep:
            std::cerr << "Sleep " << inst.arg1 << "\n";
            pcb.state = ProcessState::Blocked;
            pcb.blocked_time = inst.arg1;
            pcb.blocked_reason = BlockReason::Sleep;
            pcb.waiting_device = UINT32_MAX;
            break;
    }
}
