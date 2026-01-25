#pragma once
#include <vector>
#include <cstdint>
#include <optional>

struct FrameInfo {
    bool allocated = false;
    int owner_pid = -1;
    size_t page_number = 0;
};

class PhysicalMemory {
public:
    explicit PhysicalMemory(size_t num_frames, size_t frame_size = 4096);
    
    std::optional<uint32_t> allocate_frame(int pid, size_t page_number);
    void free_frame(uint32_t frame_number);
    
    const FrameInfo& get_frame_info(uint32_t frame_number) const;
    
    size_t get_total_frames() const { return frames_.size(); }
    size_t get_free_frames() const;
    size_t get_used_frames() const;
    
    void dump() const;
    
private:
    std::vector<FrameInfo> frames_;
    size_t frame_size_;
};
