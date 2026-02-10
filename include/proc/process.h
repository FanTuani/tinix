#include "common/config.h"
#include <cstdint>
#include <map>
#include <memory>

enum class ProcessState { New, Ready, Running, Blocked, Terminated };

class Program;

enum class BlockReason : uint8_t {
    None = 0,
    Sleep = 1,
    Device = 2,
};

struct PCB {
    int pid = -1;
    ProcessState state = ProcessState::New;
    int time_slice = config::DEFAULT_TIME_SLICE;
    int time_slice_left = config::DEFAULT_TIME_SLICE;
    int cpu_time = 0;
    int total_time = 10;
    int blocked_time = 0;
    BlockReason blocked_reason = BlockReason::None;
    uint32_t waiting_device = UINT32_MAX;
    
    std::shared_ptr<Program> program;
    size_t pc = 0;
    size_t virtual_pages = 64;

    // 进程脚本中的“逻辑 fd”映射到文件系统真实 fd。
    std::map<int, int> fd_map;
    int next_script_fd = 3;
};
