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
                  << "  exe <file>       - Execute commands from a script file\n"
                  << "  exit             - Shutdown the simulation\n";
    } else if (cmd == "ps") {
        pm_.dump_processes();
    } else if (cmd == "create") {
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
    } else if (cmd == "tick") {
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
    } else if (cmd == "exe") {
        if (args.size() > 1) {
            execute_script(args[1]);
        } else {
            std::cout << "Usage: exe <filename>\n";
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
