#include "fs/directory_manager.h"
#include <iostream>
#include <vector>
#include <cstring>

DirectoryManager::DirectoryManager(DiskDevice* disk, InodeManager* inode_mgr, BlockManager* block_mgr)
    : disk_(disk), inode_mgr_(inode_mgr), block_mgr_(block_mgr) {}

// 规范化：相对路径转换为绝对路径
std::string DirectoryManager::normalize_path(const std::string& path, const std::string& current_dir) {
    // 1) 拼接字符串
    std::string abs;
    if (path.empty()) {
        abs = current_dir.empty() ? "/" : current_dir;
    } else if (path[0] == '/') {
        abs = path;
    } else if (current_dir.empty() || current_dir == "/") {
        abs = "/" + path;
    } else {
        abs = current_dir + "/" + path;
    }

    // 语义归一（处理 . / .. / 多余的 /）
    std::vector<std::string> stack;
    std::string part;
    for (size_t i = 0; i <= abs.size(); ++i) {
        const bool at_end = (i == abs.size());
        if (at_end || abs[i] == '/') {
            if (part.empty() || part == ".") {
                // skip
            } else if (part == "..") {
                if (!stack.empty()) stack.pop_back();  // root 上的 .. 保持在 root
            } else {
                stack.push_back(part);
            }
            part.clear();
        } else {
            part.push_back(abs[i]);
        }
    }

    if (stack.empty()) return "/";

    std::string out;
    for (const auto& seg : stack) {
        out += "/";
        out += seg;
    }
    return out;
}

// 分割路径：提取父目录和文件/目录名
void DirectoryManager::split_path(const std::string& path, std::string& parent, std::string& name) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        parent = ".";
        name = path;
    } else if (pos == 0) {
        parent = "/";
        name = path.substr(1);
    } else {
        parent = path.substr(0, pos);
        name = path.substr(pos + 1);
    }
}

// 根据路径查找inode编号，逐级解析路径组件
uint32_t DirectoryManager::lookup_path(const std::string& path, const std::string& current_dir) {
    std::string norm_path = normalize_path(path, current_dir);
    
    if (norm_path == "/") {
        return ROOT_INODE;
    }
    
    uint32_t current_inode = ROOT_INODE;
    std::string remaining = norm_path.substr(1);
    
    // 逐级查找路径
    while (!remaining.empty()) {
        size_t pos = remaining.find('/');
        // 获取下一个路径组件
        std::string component = (pos == std::string::npos) ? remaining : remaining.substr(0, pos);
        
        if (component.empty() || component == ".") {
            // 跳过
        } else {
            current_inode = lookup_in_directory(current_inode, component);
            if (current_inode == INVALID_INODE) {
                return INVALID_INODE;
            }
        }
        
        if (pos == std::string::npos) break;
        remaining = remaining.substr(pos + 1);
    }
    
    return current_inode;
}

// 在指定目录中查找文件/子目录的inode编号
uint32_t DirectoryManager::lookup_in_directory(uint32_t dir_inode, const std::string& name) {
    Inode inode;
    if (!inode_mgr_->read_inode(dir_inode, inode)) {
        return INVALID_INODE;
    }
    
    if (inode.type != FileType::DIRECTORY) {
        return INVALID_INODE;
    }
    
    for (uint32_t i = 0; i < inode.blocks_used; i++) {
        std::vector<uint8_t> block_data(BLOCK_SIZE);
        if (!disk_->read_block(inode.direct_blocks[i], block_data.data())) {
            continue;
        }
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(block_data.data());
        uint32_t num_entries = BLOCK_SIZE / DIRENT_SIZE;
        
        for (uint32_t j = 0; j < num_entries; j++) {
            if (entries[j].is_valid() && name == entries[j].name) {
                return entries[j].inode_num;
            }
        }
    }
    
    return INVALID_INODE;
}

// 在目录中添加新的目录项，必要时分配新块
bool DirectoryManager::add_directory_entry(uint32_t dir_inode, const std::string& name, uint32_t inode_num) {
    Inode inode;
    if (!inode_mgr_->read_inode(dir_inode, inode)) {
        return false;
    }
    
    // 查找空闲目录项
    for (uint32_t i = 0; i < inode.blocks_used; i++) {
        std::vector<uint8_t> block_data(BLOCK_SIZE);
        if (!disk_->read_block(inode.direct_blocks[i], block_data.data())) {
            continue;
        }
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(block_data.data());
        uint32_t num_entries = BLOCK_SIZE / DIRENT_SIZE;
        
        for (uint32_t j = 0; j < num_entries; j++) {
            if (!entries[j].is_valid()) {
                entries[j] = DirectoryEntry(name.c_str(), inode_num);
                disk_->write_block(inode.direct_blocks[i], block_data.data());
                inode.size += DIRENT_SIZE;
                inode_mgr_->write_inode(dir_inode, inode);
                return true;
            }
        }
    }
    
    if (inode.blocks_used >= DIRECT_BLOCKS) {
        std::cerr << "[FS] Directory full" << std::endl;
        return false;
    }
    
    uint32_t new_block = block_mgr_->alloc_block();
    if (new_block == INVALID_BLOCK) {
        return false;
    }
    
    std::vector<uint8_t> block_data(BLOCK_SIZE, 0);
    DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(block_data.data());
    entries[0] = DirectoryEntry(name.c_str(), inode_num);
    
    disk_->write_block(new_block, block_data.data());
    inode.direct_blocks[inode.blocks_used] = new_block;
    inode.blocks_used++;
    inode.size += DIRENT_SIZE;
    inode_mgr_->write_inode(dir_inode, inode);
    
    return true;
}

// 从目录中删除指定的目录项
bool DirectoryManager::remove_directory_entry(uint32_t dir_inode, const std::string& name) {
    Inode inode;
    if (!inode_mgr_->read_inode(dir_inode, inode)) {
        return false;
    }
    
    for (uint32_t i = 0; i < inode.blocks_used; i++) {
        std::vector<uint8_t> block_data(BLOCK_SIZE);
        if (!disk_->read_block(inode.direct_blocks[i], block_data.data())) {
            continue;
        }
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(block_data.data());
        uint32_t num_entries = BLOCK_SIZE / DIRENT_SIZE;
        
        for (uint32_t j = 0; j < num_entries; j++) {
            if (entries[j].is_valid() && name == entries[j].name) {
                entries[j].inode_num = INVALID_INODE;
                disk_->write_block(inode.direct_blocks[i], block_data.data());
                inode.size -= DIRENT_SIZE;
                inode_mgr_->write_inode(dir_inode, inode);
                return true;
            }
        }
    }
    
    return false;
}

// 创建新目录：分配inode和数据块，初始化.和..
bool DirectoryManager::create_directory(const std::string& path, const std::string& current_dir) {
    std::string parent_path, dir_name;
    split_path(normalize_path(path, current_dir), parent_path, dir_name);
    
    uint32_t parent_inode = lookup_path(parent_path, current_dir);
    if (parent_inode == INVALID_INODE) {
        std::cerr << "[FS] Parent directory not found: " << parent_path << std::endl;
        return false;
    }
    
    if (lookup_in_directory(parent_inode, dir_name) != INVALID_INODE) {
        std::cerr << "[FS] Directory already exists: " << path << std::endl;
        return false;
    }
    
    uint32_t new_inode = block_mgr_->alloc_inode();
    if (new_inode == INVALID_INODE) {
        return false;
    }
    
    uint32_t data_block = block_mgr_->alloc_block();
    if (data_block == INVALID_BLOCK) {
        block_mgr_->free_inode(new_inode);
        return false;
    }
    
    Inode inode;
    inode.type = FileType::DIRECTORY;
    inode.size = 2 * DIRENT_SIZE;
    inode.blocks_used = 1;
    inode.direct_blocks[0] = data_block;
    
    std::vector<uint8_t> block_data(BLOCK_SIZE, 0);
    DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(block_data.data());
    entries[0] = DirectoryEntry(".", new_inode);
    entries[1] = DirectoryEntry("..", parent_inode);
    
    disk_->write_block(data_block, block_data.data());
    inode_mgr_->write_inode(new_inode, inode);
    
    if (!add_directory_entry(parent_inode, dir_name, new_inode)) {
        block_mgr_->free_block(data_block);
        block_mgr_->free_inode(new_inode);
        return false;
    }
    
    std::cerr << "[FS] Created directory: " << path << " (inode=" << new_inode << ")" << std::endl;
    return true;
}

// 列出目录内容：遍历所有目录项并显示
bool DirectoryManager::list_directory(const std::string& path, const std::string& current_dir) {
    uint32_t dir_inode = lookup_path(path, current_dir);
    if (dir_inode == INVALID_INODE) {
        std::cerr << "[FS] Directory not found: " << path << std::endl;
        return false;
    }
    
    Inode inode;
    if (!inode_mgr_->read_inode(dir_inode, inode)) {
        return false;
    }
    
    if (inode.type != FileType::DIRECTORY) {
        std::cerr << "[FS] Not a directory: " << path << std::endl;
        return false;
    }
    
    std::cout << "Contents of " << path << ":" << std::endl;
    
    for (uint32_t i = 0; i < inode.blocks_used; i++) {
        std::vector<uint8_t> block_data(BLOCK_SIZE);
        if (!disk_->read_block(inode.direct_blocks[i], block_data.data())) {
            continue;
        }
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(block_data.data());
        uint32_t num_entries = BLOCK_SIZE / DIRENT_SIZE;
        
        for (uint32_t j = 0; j < num_entries; j++) {
            if (entries[j].is_valid() && entries[j].name[0] != '\0') {
                Inode entry_inode;
                inode_mgr_->read_inode(entries[j].inode_num, entry_inode);
                char type = (entry_inode.type == FileType::DIRECTORY) ? 'd' : '-';
                std::cout << "  " << type << " " << entries[j].name 
                         << " (inode=" << entries[j].inode_num 
                         << ", size=" << entry_inode.size << ")" << std::endl;
            }
        }
    }
    
    return true;
}
