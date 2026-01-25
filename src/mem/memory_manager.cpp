#include "mem/memory_manager.h"
#include <iostream>
#include <iomanip>

MemoryManager::MemoryManager(size_t num_frames, size_t page_size)
    : physical_memory_(num_frames, page_size),
      page_size_(page_size) {}

// 为进程创建页表
void MemoryManager::create_process_memory(int pid, size_t num_pages) {
    page_tables_[pid] = std::make_unique<PageTable>(num_pages);
    process_stats_[pid] = MemoryStats{};
    
    std::cout << "[Memory] Created page table for PID " << pid 
              << " (" << num_pages << " pages)" << std::endl;
}

// 释放进程的所有内存（页表和物理页框）
void MemoryManager::free_process_memory(int pid) {
    auto it = page_tables_.find(pid);
    if (it == page_tables_.end()) {
        throw std::runtime_error("No page table for PID " + std::to_string(pid));
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
    
    std::cout << "[Memory] Freed memory for PID " << pid << std::endl;
}

bool MemoryManager::access_memory(int pid, uint64_t virtual_addr, AccessType type) {
    auto it = page_tables_.find(pid);
    if (it == page_tables_.end()) {
        throw std::runtime_error("No page table for PID " + std::to_string(pid));
    }
    
    // 地址转换：虚拟地址 -> 页号 + 页内偏移
    size_t page_number = virtual_addr / page_size_;
    size_t offset = virtual_addr % page_size_;
    
    PageTable* pt = it->second.get();
    if (page_number >= pt->size()) {
        std::cerr << "[Memory] Invalid address: page " << page_number << " out of range" << std::endl;
        return false;
    }
    
    stats_.memory_accesses++;
    process_stats_[pid].memory_accesses++;
    
    auto& entry = (*pt)[page_number];
    
    // 缺页：页面不在物理内存中
    if (!entry.present) {
        stats_.page_faults++;
        process_stats_[pid].page_faults++;
        
        std::cout << "[PageFault] PID=" << pid << ", VPage=" << page_number 
                  << ", VAddr=0x" << std::hex << virtual_addr << std::dec << std::endl;
        
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
    
    uint64_t physical_addr = entry.frame_number * page_size_ + offset;
    
    std::cout << "[Memory] PID=" << pid << ", VAddr=0x" << std::hex << virtual_addr 
              << " -> PAddr=0x" << physical_addr << std::dec
              << ", Frame=" << entry.frame_number << std::endl;
    
    return true;
}

bool MemoryManager::handle_page_fault(int pid, size_t page_number, AccessType type) {
    // 尝试分配空闲物理页框
    auto frame_opt = physical_memory_.allocate_frame(pid, page_number);
    
    if (!frame_opt) { // 无可用页框
        // TODO: 实现页面置换算法
        std::cerr << "[PageFault] No free frames available (TODO: page replacement)" << std::endl;
        return false;
    }
    // 分配成功
    uint32_t frame_number = *frame_opt;
    // 更新页表项
    auto& entry = (*page_tables_[pid])[page_number];
    entry.present = true;
    entry.frame_number = frame_number;
    entry.referenced = true;
    if (type == AccessType::Write) {
        entry.dirty = true;
    }
    
    std::cout << "[PageFault] Allocated Frame " << frame_number 
              << " for PID=" << pid << ", VPage=" << page_number << std::endl;
    
    return true;
}

void MemoryManager::dump_page_table(int pid) const {
    auto it = page_tables_.find(pid);
    if (it == page_tables_.end()) {
        std::cout << "PID " << pid << " has no page table" << std::endl;
        return;
    }
    
    std::cout << "=== Page Table for PID " << pid << " ===" << std::endl;
    std::cout << "VPage | Present | Frame | Dirty | Ref" << std::endl;
    std::cout << "------|---------|-------|-------|----" << std::endl;
    
    const PageTable* pt = it->second.get();
    for (size_t i = 0; i < pt->size(); ++i) {
        const auto& entry = (*pt)[i];
        std::cout << std::setw(5) << i << " |    "
                 << (entry.present ? "Y" : "N") << "    | ";
        
        if (entry.present) {
            std::cout << std::setw(5) << entry.frame_number;
        } else {
            std::cout << "  -  ";
        }
        
        std::cout << " |   " << (entry.dirty ? "Y" : "N")
                 << "   |  " << (entry.referenced ? "Y" : "N") << std::endl;
    }
    
    auto stats_it = process_stats_.find(pid);
    if (stats_it != process_stats_.end()) {
        std::cout << "\nStats: " << stats_it->second.page_faults << " page faults, "
                  << stats_it->second.memory_accesses << " accesses" << std::endl;
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
