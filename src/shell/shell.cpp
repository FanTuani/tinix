#include "shell/shell.h"
#include <iostream>
#include <sstream>
#include <fstream>

Shell::Shell(ProcessManager& pm) : pm_(pm), running_(true) {}

void Shell::run() {
    std::string line;
    std::cout << "Tinix OS Shell. Type 'help' for commands.\n";
    while (running_) {
        std::cout << "tinix> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        auto args = parse_command(line);
        if (!args.empty()) {
            execute_command(args);
        }
    }
}

std::vector<std::string> Shell::parse_command(const std::string& input) {
    std::vector<std::string> args;
    std::stringstream ss(input);
    std::string arg;
    while (ss >> arg) {
        args.push_back(arg);
    }
    return args;
}

void Shell::execute_command(const std::vector<std::string>& args) {
    const std::string& cmd = args[0];
    if (cmd == "help") {
        std::cout << "Available commands:\n"
                  << "  help             - Display this help message\n"
                  << "  ps               - List all simulated processes\n"
                  << "  create [time]    - Create a new process with optional total time (default: 10)\n"
                  << "  create -f <file> - Create a process from .pc script file\n"
                  << "  kill <pid>       - Force terminate a process\n"
                  << "  tick [n]         - Execute n clock ticks (default: 1)\n"
                  << "  run <pid>        - Manually schedule a process to run\n"
                  << "  block <pid> [t]  - Block a process for t ticks (default: 5)\n"
                  << "  wakeup <pid>     - Wake up a blocked process\n"
                  << "  pagetable <pid>  - Display page table for a process\n"
                  << "  mem              - Display physical memory status\n"
                  << "  memstats [pid]   - Display memory statistics (system or per-process)\n"
                  << "  script <file>    - Execute commands from a script file\n"
                  << "  exit             - Shutdown the simulation\n";
    } else if (cmd == "ps") {
        pm_.dump_processes();
    } else if (cmd == "create" or cmd == "cr") {
        if (args.size() > 2 && args[1] == "-f") {
            int pid = pm_.create_process_from_file(args[2]);
            if (pid != -1) {
                std::cout << "Created process PID: " << pid << " from " << args[2] << "\n";
            }
        } else {
            int total_time = 10;
            if (args.size() > 1) {
                total_time = std::stoi(args[1]);
            }
            int pid = pm_.create_process(total_time);
            std::cout << "Created process PID: " << pid << "\n";
        }
    } else if (cmd == "kill") {
        if (args.size() > 1) {
            pm_.terminate_process(std::stoi(args[1]));
        } else {
            std::cout << "Usage: kill <pid>\n";
        }
    } else if (cmd == "tick" or cmd == "tk") {
        int n = 1;
        if (args.size() > 1) {
            n = std::stoi(args[1]);
        }
        for (int i = 0; i < n; i++) {
            pm_.tick();
        }
    } else if (cmd == "run") {
        if (args.size() > 1) {
            pm_.run_process(std::stoi(args[1]));
        } else {
            std::cout << "Usage: run <pid>\n";
        }
    } else if (cmd == "block") {
        if (args.size() > 1) {
            int pid = std::stoi(args[1]);
            int duration = 5;
            if (args.size() > 2) {
                duration = std::stoi(args[2]);
            }
            pm_.block_process(pid, duration);
        } else {
            std::cout << "Usage: block <pid> [duration]\n";
        }
    } else if (cmd == "wakeup") {
        if (args.size() > 1) {
            pm_.wakeup_process(std::stoi(args[1]));
        } else {
            std::cout << "Usage: wakeup <pid>\n";
        }
    } else if (cmd == "pagetable" or cmd == "pt") {
        if (args.size() > 1) {
            int pid = std::stoi(args[1]);
            pm_.get_memory_manager().dump_page_table(pid);
        } else {
            std::cout << "Usage: pagetable <pid>\n";
        }
    } else if (cmd == "mem") {
        pm_.get_memory_manager().dump_physical_memory();
    } else if (cmd == "memstats" or cmd == "ms") {
        if (args.size() > 1) {
            int pid = std::stoi(args[1]);
            auto stats = pm_.get_memory_manager().get_process_stats(pid);
            std::cout << "=== Memory Stats for PID " << pid << " ===\n";
            std::cout << "Memory Accesses: " << stats.memory_accesses << "\n";
            std::cout << "Page Faults: " << stats.page_faults << "\n";
            if (stats.memory_accesses > 0) {
                double fault_rate = (double)stats.page_faults / stats.memory_accesses * 100.0;
                std::cout << "Page Fault Rate: " << fault_rate << "%\n";
            }
        } else {
            auto stats = pm_.get_memory_manager().get_stats();
            std::cout << "=== System Memory Stats ===\n";
            std::cout << "Total Memory Accesses: " << stats.memory_accesses << "\n";
            std::cout << "Total Page Faults: " << stats.page_faults << "\n";
            if (stats.memory_accesses > 0) {
                double fault_rate = (double)stats.page_faults / stats.memory_accesses * 100.0;
                std::cout << "Page Fault Rate: " << fault_rate << "%\n";
            }
        }
    } else if (cmd == "script" or cmd == "sc") {
        if (args.size() > 1) {
            execute_script(args[1]);
        } else {
            std::cout << "Usage: script <filename>\n";
        }
    } else if (cmd == "exit") {
        running_ = false;
    } else {
        std::cout << "Unknown command: " << cmd << "\n";
    }
}

void Shell::execute_script(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Error: Could not open script file '" << filename << "'\n";
        return;
    }

    std::cout << "Executing script: " << filename << "\n";
    std::string line;
    int line_num = 0;
    
    while (std::getline(file, line)) {
        line_num++;
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::cout << ">>> " << line << "\n";
        auto args = parse_command(line);
        if (!args.empty()) {
            execute_command(args);
        }
    }
    
    file.close();
    std::cout << "Script execution completed.\n";
}
