#pragma once
#include "physical_memory.h"
#include "page_table.h"
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
    MemoryManager(size_t num_frames = 32, size_t page_size = 4096);
    
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
    
    size_t page_size_;
    
    bool handle_page_fault(int pid, size_t page_number, AccessType type);
};
