#include "mem/physical_memory.h"
#include "common/config.h"
#include <iostream>
#include <iomanip>

PhysicalMemory::PhysicalMemory()
    : frames_(config::PAGE_FRAMES), frame_size_(config::PAGE_SIZE) {}

std::optional<size_t> PhysicalMemory::allocate_frame(int pid, size_t page_number) {
    for (size_t i = 0; i < frames_.size(); ++i) {
        if (!frames_[i].allocated) {
            frames_[i].allocated = true;
            frames_[i].owner_pid = pid;
            frames_[i].page_number = page_number;
            return i;
        }
    }
    return std::nullopt;
}

void PhysicalMemory::free_frame(size_t frame_number) {
    frames_[frame_number] = FrameInfo{};
}

void PhysicalMemory::assign_frame(size_t frame_number, int pid,
                                 size_t page_number) {
    frames_[frame_number].allocated = true;
    frames_[frame_number].owner_pid = pid;
    frames_[frame_number].page_number = page_number;
}

const FrameInfo& PhysicalMemory::get_frame_info(size_t frame_number) const {
    return frames_[frame_number];
}

size_t PhysicalMemory::get_free_frames() const {
    size_t count = 0;
    for (const auto& frame : frames_) {
        if (!frame.allocated) ++count;
    }
    return count;
}

size_t PhysicalMemory::get_used_frames() const {
    return get_total_frames() - get_free_frames();
}

void PhysicalMemory::dump() const {
    std::cerr << "=== Physical Memory ===" << std::endl;
    std::cerr << "Total: " << frames_.size() << " frames" << std::endl;
    std::cerr << "Free: " << get_free_frames() << std::endl;
    std::cerr << "Used: " << get_used_frames() << std::endl;
    
    std::cerr << "\nFrame | Status | PID | VPage" << std::endl;
    std::cerr << "------|--------|-----|------" << std::endl;
    
    for (size_t i = 0; i < frames_.size(); ++i) {
        std::cerr << std::setw(5) << i << " | ";
        if (frames_[i].allocated) {
            std::cerr << " Used  | " << std::setw(3) << frames_[i].owner_pid 
                     << " | " << std::setw(5) << frames_[i].page_number;
        } else {
            std::cerr << " Free  |  -  |   -  ";
        }
        std::cerr << std::endl;
    }
}
