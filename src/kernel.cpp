#include "kernel.h"
#include <iostream>

Kernel::Kernel() : disk_(), dev_mgr_(), fs_(&disk_), mm_(disk_), pm_(mm_, dev_mgr_, fs_) {
    // 自动挂载文件系统，如果失败则格式化
    if (!fs_.mount()) {
        std::cerr << "[Kernel] File system not found, formatting..." << std::endl;
        fs_.format();
    }
}
