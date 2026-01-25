#include "mem/page_table.h"
#include <stdexcept>

PageTable::PageTable(size_t num_pages) : entries_(num_pages) {}

PageTableEntry& PageTable::operator[](size_t page_num) {
    return entries_[page_num];
}

const PageTableEntry& PageTable::operator[](size_t page_num) const {
    return entries_[page_num];
}

void PageTable::clear() {
    for (auto& entry : entries_) {
        entry = PageTableEntry{};
    }
}
