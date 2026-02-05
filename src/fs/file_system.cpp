#include "fs/file_system.h"
#include <iostream>
#include <cstring>
#include <vector>
#include <algorithm>

// 初始化文件系统，创建各个管理器
FileSystem::FileSystem(DiskDevice* disk)
    : disk_(disk), mounted_(false), current_dir_("/") {
    inode_mgr_ = std::make_unique<InodeManager>(disk_);
    block_mgr_ = std::make_unique<BlockManager>(disk_);
    dir_mgr_ = std::make_unique<DirectoryManager>(disk_, inode_mgr_.get(), block_mgr_.get());
    fd_table_ = std::make_unique<FileDescriptorTable>();
}

// 卸载时保存修改过的数据
FileSystem::~FileSystem() {
    if (mounted_ && block_mgr_->is_bitmap_dirty()) {
        block_mgr_->save_bitmaps();
        save_superblock();
    }
}

// 格式化文件系统：初始化超级块、位图和根目录
bool FileSystem::format() {
    std::cerr << "[FS] Formatting file system..." << std::endl;
    
    // 初始化超级块
    superblock_ = SuperBlock();
    superblock_.magic = FS_MAGIC;
    superblock_.total_blocks = TOTAL_BLOCKS;
    superblock_.total_inodes = MAX_INODES;
    superblock_.free_blocks = MAX_DATA_BLOCKS;
    superblock_.free_inodes = MAX_INODES - 1;  // root已占用
    
    superblock_.inode_bitmap_block = INODE_BITMAP_BLOCK;
    superblock_.data_bitmap_block = DATA_BITMAP_BLOCK;
    superblock_.inode_table_start = INODE_TABLE_START;
    superblock_.inode_table_blocks = INODE_TABLE_BLOCKS;
    superblock_.data_blocks_start = DATA_BLOCKS_START;
    
    if (!save_superblock()) {
        std::cerr << "[FS] Format failed: unable to write SuperBlock" << std::endl;
        return false;
    }
    
    // 初始化位图
    std::vector<uint8_t> zero_bitmap(BLOCK_SIZE, 0);
    zero_bitmap[0] = 0x01;  // 标记root inode
    disk_->write_block(INODE_BITMAP_BLOCK, zero_bitmap.data());
    
    zero_bitmap[0] = 0x00;
    disk_->write_block(DATA_BITMAP_BLOCK, zero_bitmap.data());
    
    // 清空inode表
    std::vector<uint8_t> zero_block(BLOCK_SIZE, 0);
    for (uint32_t i = 0; i < INODE_TABLE_BLOCKS; i++) {
        disk_->write_block(INODE_TABLE_START + i, zero_block.data());
    }
    
    if (!init_root_directory()) {
        std::cerr << "[FS] Format failed: unable to create root directory" << std::endl;
        return false;
    }
    
    mounted_ = true;
    block_mgr_->load_bitmaps();
    
    std::cerr << "[FS] Format complete!" << std::endl;
    std::cerr << "[FS] Total blocks: " << superblock_.total_blocks 
              << ", Total inodes: " << superblock_.total_inodes << std::endl;
    
    return true;
}

// 挂载文件系统：加载超级块和位图
bool FileSystem::mount() {
    std::cerr << "[FS] Mounting file system..." << std::endl;
    
    if (!load_superblock()) {
        std::cerr << "[FS] Mount failed: unable to read SuperBlock" << std::endl;
        return false;
    }
    
    // 验证魔数
    if (superblock_.magic != FS_MAGIC) {
        std::cerr << "[FS] Mount failed: magic number mismatch (expected: 0x" 
                  << std::hex << FS_MAGIC << ", actual: 0x" 
                  << superblock_.magic << std::dec << ")" << std::endl;
        return false;
    }

    if (superblock_.total_blocks != TOTAL_BLOCKS || superblock_.total_inodes != MAX_INODES) {
        std::cerr << "[FS] Mount failed: layout mismatch, please re-format" << std::endl;
        return false;
    }
    
    if (!block_mgr_->load_bitmaps()) {
        std::cerr << "[FS] Mount failed: unable to read bitmaps" << std::endl;
        return false;
    }
    
    mounted_ = true;
    block_mgr_->set_bitmap_dirty(false);
    
    std::cerr << "[FS] Mount successful!" << std::endl;
    std::cerr << "[FS] Free blocks: " << superblock_.free_blocks 
              << ", Free inodes: " << superblock_.free_inodes << std::endl;
    
    return true;
}

// 初始化根目录：创建根inode和.、..目录项
bool FileSystem::init_root_directory() {
    Inode root_inode;
    root_inode.type = FileType::DIRECTORY;
    root_inode.size = 2 * DIRENT_SIZE;  // . 和 ..
    root_inode.blocks_used = 1;
    
    uint32_t root_data_block = block_mgr_->alloc_block();
    if (root_data_block == INVALID_BLOCK) {
        return false;
    }
    root_inode.direct_blocks[0] = root_data_block;
    
    if (!inode_mgr_->write_inode(ROOT_INODE, root_inode)) {
        return false;
    }
    
    std::vector<uint8_t> dir_block(BLOCK_SIZE, 0);
    DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(dir_block.data());
    entries[0] = DirectoryEntry(".", ROOT_INODE);
    entries[1] = DirectoryEntry("..", ROOT_INODE);
    
    if (!disk_->write_block(root_data_block, dir_block.data())) {
        return false;
    }
    
    std::cerr << "[FS] Root directory created (inode=" << ROOT_INODE 
              << ", block=" << root_data_block << ")" << std::endl;
    
    return true;
}

bool FileSystem::load_superblock() {
    std::vector<uint8_t> block_data(BLOCK_SIZE);
    if (!disk_->read_block(SUPERBLOCK_BLOCK, block_data.data())) {
        return false;
    }
    memcpy(&superblock_, block_data.data(), sizeof(SuperBlock));
    return true;
}

bool FileSystem::save_superblock() {
    std::vector<uint8_t> block_data(BLOCK_SIZE);
    memcpy(block_data.data(), &superblock_, sizeof(SuperBlock));
    return disk_->write_block(SUPERBLOCK_BLOCK, block_data.data());
}

bool FileSystem::create_directory(const std::string& path) {
    if (!mounted_) {
        std::cerr << "[FS] File system not mounted" << std::endl;
        return false;
    }
    
    bool result = dir_mgr_->create_directory(path, current_dir_);
    if (result) {
        superblock_.free_inodes--;
        superblock_.free_blocks -= 2;
        save_superblock();
        block_mgr_->save_bitmaps();
    }
    return result;
}

bool FileSystem::list_directory(const std::string& path) {
    if (!mounted_) {
        std::cerr << "[FS] File system not mounted" << std::endl;
        return false;
    }
    return dir_mgr_->list_directory(path, current_dir_);
}

bool FileSystem::change_directory(const std::string& path) {
    if (!mounted_) {
        std::cerr << "[FS] File system not mounted" << std::endl;
        return false;
    }

    uint32_t inode_num = dir_mgr_->lookup_path(path, current_dir_);
    if (inode_num == INVALID_INODE) {
        std::cerr << "[FS] Directory not found: " << path << std::endl;
        return false;
    }
    
    Inode inode;
    if (!inode_mgr_->read_inode(inode_num, inode)) {
        return false;
    }
    
    if (inode.type != FileType::DIRECTORY) {
        std::cerr << "[FS] Not a directory: " << path << std::endl;
        return false;
    }
    
    current_dir_ = dir_mgr_->normalize_path(path, current_dir_);
    
    std::cerr << "[FS] Changed directory to: " << current_dir_ << std::endl;
    return true;
}

// 创建文件：分配inode，在父目录中添加目录项
bool FileSystem::create_file(const std::string& path) {
    if (!mounted_) {
        std::cerr << "[FS] File system not mounted" << std::endl;
        return false;
    }
    
    // 分离父目录和文件名
    std::string parent_path, file_name;
    dir_mgr_->split_path(dir_mgr_->normalize_path(path, current_dir_), parent_path, file_name);
    
    uint32_t parent_inode = dir_mgr_->lookup_path(parent_path, current_dir_);
    if (parent_inode == INVALID_INODE) {
        std::cerr << "[FS] Parent directory not found: " << parent_path << std::endl;
        return false;
    }
    
    if (dir_mgr_->lookup_in_directory(parent_inode, file_name) != INVALID_INODE) {
        std::cerr << "[FS] File already exists: " << path << std::endl;
        return false;
    }
    
    uint32_t new_inode = block_mgr_->alloc_inode();
    if (new_inode == INVALID_INODE) {
        return false;
    }
    
    Inode inode;
    inode.type = FileType::REGULAR;
    inode.size = 0;
    inode.blocks_used = 0;
    
    inode_mgr_->write_inode(new_inode, inode);
    
    if (!dir_mgr_->add_directory_entry(parent_inode, file_name, new_inode)) {
        block_mgr_->free_inode(new_inode);
        return false;
    }
    
    superblock_.free_inodes--;
    save_superblock();
    block_mgr_->save_bitmaps();
    
    std::cerr << "[FS] Created file: " << path << " (inode=" << new_inode << ")" << std::endl;
    return true;
}

// 删除文件：释放所有数据块和inode
bool FileSystem::remove_file(const std::string& path) {
    if (!mounted_) {
        std::cerr << "[FS] File system not mounted" << std::endl;
        return false;
    }
    
    std::string parent_path, file_name;
    dir_mgr_->split_path(dir_mgr_->normalize_path(path, current_dir_), parent_path, file_name);
    
    uint32_t parent_inode = dir_mgr_->lookup_path(parent_path, current_dir_);
    if (parent_inode == INVALID_INODE) {
        return false;
    }
    
    uint32_t file_inode = dir_mgr_->lookup_in_directory(parent_inode, file_name);
    if (file_inode == INVALID_INODE) {
        std::cerr << "[FS] File not found: " << path << std::endl;
        return false;
    }
    
    Inode inode;
    if (!inode_mgr_->read_inode(file_inode, inode)) {
        return false;
    }
    
    for (uint32_t i = 0; i < inode.blocks_used; i++) {
        block_mgr_->free_block(inode.direct_blocks[i]);
    }
    
    block_mgr_->free_inode(file_inode);
    dir_mgr_->remove_directory_entry(parent_inode, file_name);
    
    superblock_.free_inodes++;
    superblock_.free_blocks += inode.blocks_used;
    save_superblock();
    block_mgr_->save_bitmaps();
    
    std::cerr << "[FS] Removed file: " << path << std::endl;
    return true;
}

int FileSystem::open_file(const std::string& path) {
    if (!mounted_) {
        std::cerr << "[FS] File system not mounted" << std::endl;
        return -1;
    }
    
    uint32_t inode_num = dir_mgr_->lookup_path(path, current_dir_);
    if (inode_num == INVALID_INODE) {
        std::cerr << "[FS] File not found: " << path << std::endl;
        return -1;
    }
    
    Inode inode;
    if (!inode_mgr_->read_inode(inode_num, inode)) {
        return -1;
    }
    
    if (inode.type != FileType::REGULAR) {
        std::cerr << "[FS] Not a regular file: " << path << std::endl;
        return -1;
    }
    
    int fd = fd_table_->alloc_fd(inode_num);
    std::cerr << "[FS] Opened file: " << path << " (fd=" << fd << ")" << std::endl;
    return fd;
}

void FileSystem::close_file(int fd) {
    if (fd_table_->free_fd(fd)) {
        std::cerr << "[FS] Closed file (fd=" << fd << ")" << std::endl;
    }
}

ssize_t FileSystem::read_file(int fd, void* buffer, size_t size) {
    OpenFile* file = fd_table_->get_open_file(fd);
    if (!file) {
        std::cerr << "[FS] Invalid file descriptor: " << fd << std::endl;
        return -1;
    }
    
    Inode inode;
    if (!inode_mgr_->read_inode(file->inode_num, inode)) {
        return -1;
    }
    
    size_t available = (file->offset < inode.size) ? (inode.size - file->offset) : 0;
    size_t to_read = std::min(size, available);
    
    if (to_read == 0) {
        return 0;
    }
    
    size_t bytes_read = 0;
    uint8_t* buf = static_cast<uint8_t*>(buffer);
    
    // 按块读取数据
    while (bytes_read < to_read) {
        uint32_t block_idx = file->offset / BLOCK_SIZE;
        uint32_t block_offset = file->offset % BLOCK_SIZE;
        
        if (block_idx >= inode.blocks_used) {
            break;
        }
        
        std::vector<uint8_t> block_data(BLOCK_SIZE);
        if (!disk_->read_block(inode.direct_blocks[block_idx], block_data.data())) {
            break;
        }
        
        size_t chunk = std::min(to_read - bytes_read, static_cast<size_t>(BLOCK_SIZE - block_offset));
        memcpy(buf + bytes_read, block_data.data() + block_offset, chunk);
        
        bytes_read += chunk;
        file->offset += chunk;
    }
    
    return bytes_read;
}

ssize_t FileSystem::write_file(int fd, const void* buffer, size_t size) {
    OpenFile* file = fd_table_->get_open_file(fd);
    if (!file) {
        std::cerr << "[FS] Invalid file descriptor: " << fd << std::endl;
        return -1;
    }
    
    Inode inode;
    if (!inode_mgr_->read_inode(file->inode_num, inode)) {
        return -1;
    }
    
    size_t bytes_written = 0;
    const uint8_t* buf = static_cast<const uint8_t*>(buffer);
    
    // 按块写入数据，必要时分配新块
    while (bytes_written < size) {
        uint32_t block_idx = file->offset / BLOCK_SIZE;
        uint32_t block_offset = file->offset % BLOCK_SIZE;
        
        // 需要新块时分配
        if (block_idx >= inode.blocks_used) {
            if (block_idx >= DIRECT_BLOCKS) {
                std::cerr << "[FS] File size limit reached" << std::endl;
                break;
            }
            
            uint32_t new_block = block_mgr_->alloc_block();
            if (new_block == INVALID_BLOCK) {
                break;
            }
            
            inode.direct_blocks[block_idx] = new_block;
            inode.blocks_used++;
            superblock_.free_blocks--;
        }
        
        std::vector<uint8_t> block_data(BLOCK_SIZE, 0);
        
        // 部分块写入时需要先读取原数据
        if (block_offset != 0 || (size - bytes_written) < BLOCK_SIZE) {
            disk_->read_block(inode.direct_blocks[block_idx], block_data.data());
        }
        
        size_t chunk = std::min(size - bytes_written, static_cast<size_t>(BLOCK_SIZE - block_offset));
        memcpy(block_data.data() + block_offset, buf + bytes_written, chunk);
        
        disk_->write_block(inode.direct_blocks[block_idx], block_data.data());
        
        bytes_written += chunk;
        file->offset += chunk;
        
        if (file->offset > inode.size) {
            inode.size = file->offset;
        }
    }
    
    inode_mgr_->write_inode(file->inode_num, inode);
    save_superblock();
    block_mgr_->save_bitmaps();
    
    return bytes_written;
}

void FileSystem::print_superblock() const {
    std::cerr << "========== SuperBlock ==========" << std::endl;
    std::cerr << "Magic: 0x" << std::hex << superblock_.magic << std::dec << std::endl;
    std::cerr << "Total blocks: " << superblock_.total_blocks << std::endl;
    std::cerr << "Total inodes: " << superblock_.total_inodes << std::endl;
    std::cerr << "Free blocks: " << superblock_.free_blocks << std::endl;
    std::cerr << "Free inodes: " << superblock_.free_inodes << std::endl;
    std::cerr << "Data blocks start: " << superblock_.data_blocks_start << std::endl;
    std::cerr << "===============================" << std::endl;
}

void FileSystem::print_inode(uint32_t inode_num) const {
    Inode inode;
    if (!inode_mgr_->read_inode(inode_num, inode)) {
        return;
    }
    
    std::cerr << "========== Inode " << inode_num << " ==========" << std::endl;
    std::cerr << "Type: " << (inode.type == FileType::DIRECTORY ? "Directory" : "File") << std::endl;
    std::cerr << "Size: " << inode.size << " bytes" << std::endl;
    std::cerr << "Blocks used: " << inode.blocks_used << std::endl;
    std::cerr << "Direct blocks: ";
    for (uint32_t i = 0; i < inode.blocks_used && i < DIRECT_BLOCKS; i++) {
        std::cerr << inode.direct_blocks[i] << " ";
    }
    std::cerr << std::endl;
    std::cerr << "===============================" << std::endl;
}
