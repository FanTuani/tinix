#include "dev/disk.h"
#include <iostream>
#include <vector>
#include <filesystem>

DiskDevice::DiskDevice() {
    initialize_disk();
}

DiskDevice::~DiskDevice() {
    if (disk_file_.is_open()) {
        disk_file_.close();
    }
}

void DiskDevice::initialize_disk() {
    // 检查磁盘文件是否存在，不存在则创建并预分配空间
    if (!std::filesystem::exists(filename_)) {
        std::cerr << "[Disk] Creating new disk image: " << filename_ 
                  << " (" << (num_blocks_ * block_size_) / 1024 << " KB)" << std::endl;
        
        std::ofstream outfile(filename_, std::ios::binary | std::ios::out);
        std::vector<uint8_t> empty_block(block_size_, 0);
        for (size_t i = 0; i < num_blocks_; ++i) {
            outfile.write(reinterpret_cast<const char*>(empty_block.data()), block_size_);
        }
        outfile.close();
    }

    std::cerr << "[Disk] Opening disk image: " << filename_ << std::endl;
    // 以读写模式打开
    disk_file_.open(filename_, std::ios::binary | std::ios::in | std::ios::out);
    if (!disk_file_.is_open()) {
        std::cerr << "[Disk] Error: Could not open disk image " << filename_ << std::endl;
    }
}

bool DiskDevice::read_block(size_t block_id, uint8_t* out_buffer) {
    if (block_id >= num_blocks_) {
        throw std::runtime_error("Read error: block_id " + std::to_string(block_id) + " out of range");
    }

    disk_file_.seekg(block_id * block_size_, std::ios::beg);
    disk_file_.read(reinterpret_cast<char*>(out_buffer), block_size_);
    
    return disk_file_.good();
}

bool DiskDevice::write_block(size_t block_id, const uint8_t* in_buffer) {
    if (block_id >= num_blocks_) {
        throw std::runtime_error("Write error: block_id " + std::to_string(block_id) + " out of range");
    }

    disk_file_.seekp(block_id * block_size_, std::ios::beg);
    disk_file_.write(reinterpret_cast<const char*>(in_buffer), block_size_);
    disk_file_.flush(); // 确保写入物理设备
    
    return disk_file_.good();
}
