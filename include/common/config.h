#pragma once
#include <cstddef>

namespace config {

// mem
constexpr size_t PAGE_FRAMES = 8;              // 物理内存页框数
constexpr size_t PAGE_SIZE = 0x1000;           // 页大小 4 KB
constexpr size_t DEFAULT_VIRTUAL_PAGES = 256;  // 每个进程虚拟空间页数

// disk
constexpr const char* DISK_IMAGE_NAME = "disk.img";
constexpr size_t DISK_BLOCK_SIZE = 0x1000;  // 块大小 4 KB
constexpr size_t DISK_NUM_BLOCKS = 1024;    // 总块数

// swap
constexpr size_t SWAP_RESERVED_BLOCKS = 128;
constexpr size_t SWAP_START_BLOCK = DISK_NUM_BLOCKS - SWAP_RESERVED_BLOCKS;

static_assert(SWAP_RESERVED_BLOCKS < DISK_NUM_BLOCKS);

// proc
constexpr int DEFAULT_TIME_SLICE = 3;  // 时间片长度
}
