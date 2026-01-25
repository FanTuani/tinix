#pragma once
#include <string>
#include <vector>
#include "../proc/process_manager.h"
#include "../mem/memory_manager.h"

class Shell {
public:
    explicit Shell(ProcessManager& pm);
    void run();

private:
    ProcessManager& pm_;
    bool running_;

    std::vector<std::string> parse_command(const std::string& input);
    void execute_command(const std::vector<std::string>& args);
    void execute_script(const std::string& filename);
};
