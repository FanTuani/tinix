#include "mem/memory_manager.h"
#include <iomanip>
#include <iostream>
#include <stdexcept>

MemoryManager::MemoryManager(DiskDevice& disk)
    : physical_memory_(), disk_(disk) {}

// 为进程创建页表
void MemoryManager::create_process_memory(int pid, size_t num_pages) {
    page_tables_[pid] = std::make_unique<PageTable>(num_pages);
    process_stats_[pid] = MemoryStats{};

    std::cerr << "[Memory] Created page table for PID " << pid << " ("
              << num_pages << " pages)" << std::endl;
}

// 释放进程的所有内存（页表和物理页框）
void MemoryManager::free_process_memory(int pid) {
    auto it = page_tables_.find(pid);
    if (it == page_tables_.end()) {
        throw std::runtime_error("No page table for PID " +
                                 std::to_string(pid));
    }

    // 释放所有已分配的物理页框
    PageTable* pt = it->second.get();
    for (size_t i = 0; i < pt->size(); ++i) {
        auto& entry = (*pt)[i];
        if (entry.present) {
            physical_memory_.free_frame(entry.frame_number);
        }
    }

    // 删除页表和统计信息
    page_tables_.erase(it);
    process_stats_.erase(pid);

    std::cerr << "[Memory] Freed memory for PID " << pid << std::endl;
}

bool MemoryManager::access_memory(int pid,
                                  uint64_t virtual_addr,
                                  AccessType type) {
    auto it = page_tables_.find(pid);
    if (it == page_tables_.end()) {
        throw std::runtime_error("No page table for PID " +
                                 std::to_string(pid));
    }

    // 地址转换
    size_t page_number = virtual_addr / page_size_;
    size_t offset = virtual_addr % page_size_;

    PageTable* pt = it->second.get();
    if (page_number >= pt->size()) {
        std::cerr << "[Memory] Invalid address: page " << page_number
                  << " out of range" << std::endl;
        return false;
    }

    stats_.memory_accesses++;
    process_stats_[pid].memory_accesses++;

    auto& entry = (*pt)[page_number];

    // 缺页
    if (!entry.present) {
        stats_.page_faults++;
        process_stats_[pid].page_faults++;

        std::cerr << "[PageFault] PID=" << pid << ", VPage=" << page_number
                  << ", VAddr=0x" << std::hex << virtual_addr << std::dec
                  << std::endl;

        // 处理缺页
        if (!handle_page_fault(pid, page_number, type)) {
            return false;
        }
    }

    // 更新页表项标志位
    entry.referenced = true;
    if (type == AccessType::Write) {
        entry.dirty = true;
    }

    uint64_t physical_addr = (uint64_t)entry.frame_number * page_size_ + offset;

    std::cerr << "[Memory] PID=" << pid << ", VAddr=0x" << std::hex
              << virtual_addr << " -> PAddr=0x" << physical_addr << std::dec
              << ", Frame=" << entry.frame_number << std::endl;

    return true;
}

bool MemoryManager::handle_page_fault(int pid,
                                      size_t page_number,
                                      AccessType type) {
    auto& entry = (*page_tables_[pid])[page_number];

    if (entry.on_disk) {
        std::cerr << "[Swap] Reading PID=" << pid << " VPage=" << page_number
                  << " from Disk Block " << entry.swap_block << std::endl;

        // 使用哑数据模拟换入
        std::vector<uint8_t> dummy_data(page_size_);
        disk_.read_block(entry.swap_block, dummy_data.data());
    }

    // 尝试分配空闲物理页框
    auto frame_opt = physical_memory_.allocate_frame(pid, page_number);

    size_t frame_number = 0;  // 分配到的页框号

    if (frame_opt) {  // 有可用页框
        frame_number = *frame_opt;
    } else {
        // 无可用页框：使用 Clock 进行页面置换
        const size_t total_frames = physical_memory_.get_total_frames();

        while (true) {
            const auto& frame_info =
                physical_memory_.get_frame_info(clock_ptr_);

            if (!frame_info.allocated) {
                throw std::runtime_error("Clock pointer points to free frame");
            }

            const int victim_pid = frame_info.owner_pid;
            const size_t victim_vpage = frame_info.page_number;

            auto vpt_it =
                page_tables_.find(victim_pid);  // vpt = victim_page_table
            if (vpt_it == page_tables_.end()) {
                throw std::runtime_error("No page table for victim PID " +
                                         std::to_string(victim_pid));
            }

            PageTable& vpt = *(vpt_it->second);

            auto& victim_entry = vpt[victim_vpage];

            if (victim_entry.referenced) {
                victim_entry.referenced = false;  // second chance
                clock_ptr_ = (clock_ptr_ + 1) % total_frames;
            } else {
                // 牺牲
                std::cerr << "[Evict] Replacing Frame " << clock_ptr_
                          << " from PID=" << victim_pid
                          << ", VPage=" << victim_vpage << std::endl;

                if (victim_entry.dirty) {
                    // 脏页写回磁盘（交换出）
                    if (!victim_entry.on_disk) {
                        if (next_swap_block_ >= config::DISK_NUM_BLOCKS) {
                            std::cerr << "[Swap] Out of swap blocks" << std::endl;
                            return false;
                        }
                        victim_entry.swap_block = next_swap_block_++;
                        victim_entry.on_disk = true;
                    }

                    std::cerr << "[Swap] Writing PID=" << victim_pid
                              << " VPage=" << victim_vpage << " to Disk Block "
                              << victim_entry.swap_block << std::endl;

                    // 使用哑数据模拟写回
                    std::vector<uint8_t> dummy_data(page_size_,
                                                    0xAA);  // 0xAA 表示标记数据
                    disk_.write_block(victim_entry.swap_block,
                                      dummy_data.data());
                }

                victim_entry.clear();
                physical_memory_.assign_frame(clock_ptr_, pid, page_number);
                frame_number = clock_ptr_;
                clock_ptr_ = (clock_ptr_ + 1) % total_frames;
                break;
            }
        }
    }

    // 更新缺页进程的页表项
    entry.present = true;
    entry.frame_number = frame_number;
    entry.referenced = true;
    entry.dirty = (type == AccessType::Write);

    std::cerr << "[PageFault] Allocated Frame " << frame_number
              << " for PID=" << pid << ", VPage=" << page_number << std::endl;

    return true;
}

void MemoryManager::dump_page_table(int pid) const {
    auto it = page_tables_.find(pid);
    if (it == page_tables_.end()) {
        std::cerr << "PID " << pid << " has no page table" << std::endl;
        return;
    }

    std::cerr << "=== Page Table for PID " << pid << " ===" << std::endl;
    std::cerr << "VPage | Present | Frame | Dirty | Ref" << std::endl;
    std::cerr << "------|---------|-------|-------|----" << std::endl;

    const PageTable* pt = it->second.get();
    for (size_t i = 0; i < pt->size(); ++i) {
        const auto& entry = (*pt)[i];
        std::cerr << std::setw(5) << i << " |    " << entry.present << "    | ";

        if (entry.present) {
            std::cerr << std::setw(5) << entry.frame_number;
        } else {
            std::cerr << "  -  ";
        }

        std::cerr << " |   " << entry.dirty << "   |  " << entry.referenced
                  << std::endl;
    }

    auto stats_it = process_stats_.find(pid);
    if (stats_it != process_stats_.end()) {
        std::cerr << "\nStats: " << stats_it->second.page_faults
                  << " page faults, " << stats_it->second.memory_accesses
                  << " accesses" << std::endl;
    }
}

void MemoryManager::dump_physical_memory() const {
    physical_memory_.dump();
}

MemoryStats MemoryManager::get_process_stats(int pid) const {
    auto it = process_stats_.find(pid);
    if (it != process_stats_.end()) {
        return it->second;
    }
    return MemoryStats{};
}

void MemoryManager::reset_stats() {
    stats_ = MemoryStats{};
    process_stats_.clear();
}
