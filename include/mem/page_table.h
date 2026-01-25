#pragma once
#include <cstdint>
#include <vector>

struct PageTableEntry {
    bool present = false;
    uint32_t frame_number = 0;
    bool dirty = false;
    bool referenced = false;
};

class PageTable {
public:
    explicit PageTable(size_t num_pages);
    
    PageTableEntry& operator[](size_t page_num);
    const PageTableEntry& operator[](size_t page_num) const;
    
    size_t size() const { return entries_.size(); }
    void clear();
    
private:
    std::vector<PageTableEntry> entries_;
};
