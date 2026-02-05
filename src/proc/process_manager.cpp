#include "proc/process_manager.h"
#include <iostream>
#include "proc/program.h"
#include "common/config.h"

ProcessManager::ProcessManager(MemoryManager& memory_manager)
    : memory_manager_(memory_manager) {}

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
    if (processes_.find(pid) == processes_.end()) {
        std::cerr << "Process " << pid << " not found.\n";
        return;
    }
    
    // 释放进程的内存资源
    memory_manager_.free_process_memory(pid);
    
    processes_.erase(pid);
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
    ready_queue_.push(pid);
    std::cerr << "Process " << pid << " woken up and added to ready queue\n";
}

void ProcessManager::check_blocked_processes() {
    // 更新阻塞进程的阻塞时间
    for (auto& [pid, pcb] : processes_) {
        if (pcb.state == ProcessState::Blocked && pcb.blocked_time > 0) {
            pcb.blocked_time--;
            if (pcb.blocked_time <= 0) {
                pcb.state = ProcessState::Ready;
                ready_queue_.push(pid);
                std::cerr << "[Tick] Process " << pid << " auto-woken up\n";
            }
        }
    }
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
        case OpType::FileOpen:
            std::cerr << "FileOpen file=" << inst.str_arg << "\n";
            break;
        case OpType::FileClose:
            std::cerr << "FileClose fd=" << inst.arg1 << "\n";
            break;
        case OpType::FileRead:
            std::cerr << "FileRead fd=" << inst.arg1 << " size=" << inst.arg2
                      << "\n";
            break;
        case OpType::FileWrite:
            std::cerr << "FileWrite fd=" << inst.arg1 << " size=" << inst.arg2
                      << "\n";
            break;
        case OpType::DevRequest:
            std::cerr << "DevRequest dev=" << inst.arg1 << "\n";
            break;
        case OpType::DevRelease:
            std::cerr << "DevRelease dev=" << inst.arg1 << "\n";
            break;
        case OpType::Sleep:
            std::cerr << "Sleep " << inst.arg1 << "\n";
            pcb.state = ProcessState::Blocked;
            pcb.blocked_time = inst.arg1;
            break;
    }
}
