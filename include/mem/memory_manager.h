#pragma once
#include "physical_memory.h"
#include "page_table.h"
#include "dev/disk.h"
#include "common/config.h"
#include <map>
#include <memory>
#include <cstdint>

enum class AccessType {
    Read,
    Write
};

struct MemoryStats {
    size_t page_faults = 0;
    size_t memory_accesses = 0;
};

class MemoryManager {
public:
    MemoryManager(DiskDevice& disk);
    
    void create_process_memory(int pid, size_t num_pages);
    void free_process_memory(int pid);
    
    bool access_memory(int pid, uint64_t virtual_addr, AccessType type);
    
    void dump_page_table(int pid) const;
    void dump_physical_memory() const;
    
    const MemoryStats& get_stats() const { return stats_; }
    MemoryStats get_process_stats(int pid) const;
    void reset_stats();
    
private:
    PhysicalMemory physical_memory_;
    std::map<int, std::unique_ptr<PageTable>> page_tables_;
    std::map<int, MemoryStats> process_stats_;
    MemoryStats stats_;
    DiskDevice& disk_;
    
    size_t page_size_ = config::PAGE_SIZE;
    size_t clock_ptr_ = 0;
    size_t next_swap_block_ = config::SWAP_START_BLOCK;

    bool handle_page_fault(int pid, size_t page_number, AccessType type);
};
