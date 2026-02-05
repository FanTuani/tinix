#pragma once
#include "common/config.h"
#include <cstdint>
#include <cstring>

// 文件系统布局常量
constexpr uint32_t BLOCK_SIZE = static_cast<uint32_t>(config::DISK_BLOCK_SIZE);
// FS: [0, SWAP_START_BLOCK), swap: [SWAP_START_BLOCK, DISK_NUM_BLOCKS)
constexpr uint32_t TOTAL_BLOCKS = static_cast<uint32_t>(config::SWAP_START_BLOCK);

// 布局设计
constexpr uint32_t SUPERBLOCK_BLOCK = 0;
constexpr uint32_t INODE_BITMAP_BLOCK = 1;
constexpr uint32_t DATA_BITMAP_BLOCK = 2;
constexpr uint32_t INODE_TABLE_START = 3; 
constexpr uint32_t INODE_TABLE_BLOCKS = 4;
constexpr uint32_t DATA_BLOCKS_START = 7;

// 容量设计
constexpr uint32_t MAX_INODES = 128;
constexpr uint32_t MAX_DATA_BLOCKS = TOTAL_BLOCKS - DATA_BLOCKS_START;

// Inode 配置
constexpr uint32_t DIRECT_BLOCKS = 10;  // 每个inode有10个直接块指针
constexpr uint32_t MAX_FILE_SIZE = DIRECT_BLOCKS * BLOCK_SIZE;  // 40KB

// 目录项配置
constexpr uint32_t MAX_FILENAME_LEN = 28;
constexpr uint32_t DIRENT_SIZE = 32;  // 28 bytes name + 4 bytes inode_num

// 特殊 inode 编号
constexpr uint32_t ROOT_INODE = 0;
constexpr uint32_t INVALID_INODE = 0xFFFFFFFF;
constexpr uint32_t INVALID_BLOCK = 0xFFFFFFFF;

// 文件系统魔数
constexpr uint32_t FS_MAGIC = 0x54494E58;  // "TINX"

// 文件类型
enum class FileType : uint8_t {
    REGULAR = 1,
    DIRECTORY = 2
};

// SuperBlock 结构 (占用一个块)
struct SuperBlock {
    uint32_t magic;                   // 魔数，用于识别文件系统
    uint32_t total_blocks;            // 总块数
    uint32_t total_inodes;            // 总inode数
    uint32_t free_blocks;             // 空闲块数
    uint32_t free_inodes;             // 空闲inode数
    
    uint32_t inode_bitmap_block;      // inode位图起始块号
    uint32_t data_bitmap_block;       // 数据块位图起始块号
    uint32_t inode_table_start;       // inode表起始块号
    uint32_t inode_table_blocks;      // inode表占用块数
    uint32_t data_blocks_start;       // 数据块起始块号
    
    uint8_t padding[BLOCK_SIZE - 40]; // 填充至4096字节
    
    SuperBlock() {
        memset(this, 0, sizeof(SuperBlock));
    }
};

// Inode 结构 (128 bytes, 每个块可存放32个inode)
struct Inode {
    FileType type;                   // 文件类型
    uint8_t padding1[3];             // 对齐
    uint32_t size;                   // 文件大小（字节）
    uint32_t blocks_used;            // 已使用的数据块数
    uint32_t direct_blocks[DIRECT_BLOCKS];  // 直接块指针
    uint8_t padding2[128 - 4 - 4 - 4 - DIRECT_BLOCKS * 4];  // 填充至128字节
    
    Inode() {
        memset(this, 0, sizeof(Inode));
        type = FileType::REGULAR;
        for (auto& block : direct_blocks) {
            block = INVALID_BLOCK;
        }
    }
};

// 目录项结构 (32 bytes, 每个块可存放128个目录项)
struct DirectoryEntry {
    char name[MAX_FILENAME_LEN];     // 文件名
    uint32_t inode_num;              // inode编号
    
    DirectoryEntry() {
        memset(name, 0, MAX_FILENAME_LEN);
        inode_num = INVALID_INODE;
    }
    
    DirectoryEntry(const char* filename, uint32_t ino) {
        memset(name, 0, MAX_FILENAME_LEN);
        strncpy(name, filename, MAX_FILENAME_LEN - 1);
        inode_num = ino;
    }
    
    bool is_valid() const {
        return inode_num != INVALID_INODE;
    }
};

static_assert(sizeof(SuperBlock) == BLOCK_SIZE, "SuperBlock size must equal BLOCK_SIZE");
static_assert(sizeof(Inode) == 128, "Inode size must be 128 bytes");
static_assert(sizeof(DirectoryEntry) == DIRENT_SIZE, "DirectoryEntry size must equal DIRENT_SIZE");
static_assert(TOTAL_BLOCKS > DATA_BLOCKS_START, "FS partition too small");
