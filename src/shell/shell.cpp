#include "shell/shell.h"
#include "kernel.h"
#include <iostream>
#include <sstream>
#include <fstream>

Shell::Shell(Kernel& kernel) : kernel_(kernel), running_(true) {}

void Shell::run() {
    std::string line;
    std::cerr << "Tinix OS Shell. Type 'help' for commands.\n";
    while (running_) {
        std::cerr << "tinix> ";
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
                  << "\n"
                  << "  === File System Commands ===\n"
                  << "  format           - Format the file system\n"
                  << "  mount            - Mount the file system\n"
                  << "  touch <file>     - Create a new file\n"
                  << "  mkdir <dir>      - Create a new directory\n"
                  << "  ls [path]        - List directory contents\n"
                  << "  cd <path>        - Change current directory\n"
                  << "  pwd              - Print working directory\n"
                  << "  rm <file>        - Remove a file\n"
                  << "  cat <file>       - Display file contents\n"
                  << "  echo <text>      - Write text to file (use > for redirection)\n"
                  << "  fsinfo           - Display file system information\n"
                  << "\n"
                  << "  exit             - Shutdown the simulation\n";
    } else if (cmd == "ps") {
        kernel_.get_process_manager().dump_processes();
    } else if (cmd == "create" or cmd == "cr") {
        if (args.size() > 2 && args[1] == "-f") {
            int pid = kernel_.get_process_manager().create_process_from_file(args[2]);
            if (pid != -1) {
                std::cerr << "Created process PID: " << pid << " from " << args[2] << "\n";
            }
        } else {
            int total_time = 10;
            if (args.size() > 1) {
                total_time = std::stoi(args[1]);
            }
            int pid = kernel_.get_process_manager().create_process(total_time);
            std::cerr << "Created process PID: " << pid << "\n";
        }
    } else if (cmd == "kill") {
        if (args.size() > 1) {
            kernel_.get_process_manager().terminate_process(std::stoi(args[1]));
        } else {
            std::cerr << "Usage: kill <pid>\n";
        }
    } else if (cmd == "tick" or cmd == "tk") {
        int n = 1;
        if (args.size() > 1) {
            n = std::stoi(args[1]);
        }
        for (int i = 0; i < n; i++) {
            kernel_.get_process_manager().tick();
        }
    } else if (cmd == "run") {
        if (args.size() > 1) {
            kernel_.get_process_manager().run_process(std::stoi(args[1]));
        } else {
            std::cerr << "Usage: run <pid>\n";
        }
    } else if (cmd == "block") {
        if (args.size() > 1) {
            int pid = std::stoi(args[1]);
            int duration = 5;
            if (args.size() > 2) {
                duration = std::stoi(args[2]);
            }
            kernel_.get_process_manager().block_process(pid, duration);
        } else {
            std::cerr << "Usage: block <pid> [duration]\n";
        }
    } else if (cmd == "wakeup") {
        if (args.size() > 1) {
            kernel_.get_process_manager().wakeup_process(std::stoi(args[1]));
        } else {
            std::cerr << "Usage: wakeup <pid>\n";
        }
    } else if (cmd == "pagetable" or cmd == "pt") {
        if (args.size() > 1) {
            int pid = std::stoi(args[1]);
            kernel_.get_memory_manager().dump_page_table(pid);
        } else {
            std::cerr << "Usage: pagetable <pid>\n";
        }
    } else if (cmd == "mem") {
        kernel_.get_memory_manager().dump_physical_memory();
    } else if (cmd == "memstats" or cmd == "ms") {
        if (args.size() > 1) {
            int pid = std::stoi(args[1]);
            auto stats = kernel_.get_memory_manager().get_process_stats(pid);
            std::cerr << "=== Memory Stats for PID " << pid << " ===\n";
            std::cerr << "Memory Accesses: " << stats.memory_accesses << "\n";
            std::cerr << "Page Faults: " << stats.page_faults << "\n";
            if (stats.memory_accesses > 0) {
                double fault_rate = (double)stats.page_faults / stats.memory_accesses * 100.0;
                std::cerr << "Page Fault Rate: " << fault_rate << "%\n";
            }
        } else {
            auto stats = kernel_.get_memory_manager().get_stats();
            std::cerr << "=== System Memory Stats ===\n";
            std::cerr << "Total Memory Accesses: " << stats.memory_accesses << "\n";
            std::cerr << "Total Page Faults: " << stats.page_faults << "\n";
            if (stats.memory_accesses > 0) {
                double fault_rate = (double)stats.page_faults / stats.memory_accesses * 100.0;
                std::cerr << "Page Fault Rate: " << fault_rate << "%\n";
            }
        }
    } else if (cmd == "script" or cmd == "sc") {
        if (args.size() > 1) {
            execute_script(args[1]);
        } else {
            std::cerr << "Usage: script <filename>\n";
        }
    
    // === File System Commands ===
    } else if (cmd == "format") {
        if (kernel_.get_file_system().format()) {
            std::cerr << "File system formatted successfully.\n";
        } else {
            std::cerr << "Failed to format file system.\n";
        }
    } else if (cmd == "mount") {
        if (kernel_.get_file_system().mount()) {
            std::cerr << "File system mounted successfully.\n";
        } else {
            std::cerr << "Failed to mount file system.\n";
        }
    } else if (cmd == "touch") {
        if (args.size() > 1) {
            kernel_.get_file_system().create_file(args[1]);
        } else {
            std::cerr << "Usage: touch <filename>\n";
        }
    } else if (cmd == "mkdir") {
        if (args.size() > 1) {
            kernel_.get_file_system().create_directory(args[1]);
        } else {
            std::cerr << "Usage: mkdir <dirname>\n";
        }
    } else if (cmd == "ls") {
        std::string path = (args.size() > 1) ? args[1] : ".";
        kernel_.get_file_system().list_directory(path);
    } else if (cmd == "cd") {
        if (args.size() > 1) {
            kernel_.get_file_system().change_directory(args[1]);
        } else {
            kernel_.get_file_system().change_directory("/");
        }
    } else if (cmd == "pwd") {
        std::cout << kernel_.get_file_system().get_current_directory() << "\n";
    } else if (cmd == "rm") {
        if (args.size() > 1) {
            kernel_.get_file_system().remove_file(args[1]);
        } else {
            std::cerr << "Usage: rm <filename>\n";
        }
    } else if (cmd == "cat") {
        if (args.size() > 1) {
            int fd = kernel_.get_file_system().open_file(args[1]);
            if (fd >= 0) {
                std::vector<char> buffer(4096);
                ssize_t bytes_read = kernel_.get_file_system().read_file(fd, buffer.data(), buffer.size());
                if (bytes_read > 0) {
                    std::cout.write(buffer.data(), bytes_read);
                    std::cout << "\n";
                }
                kernel_.get_file_system().close_file(fd);
            }
        } else {
            std::cerr << "Usage: cat <filename>\n";
        }
    } else if (cmd == "echo") {
        if (args.size() < 2) {
            std::cerr << "Usage: echo <text> [> filename]\n";
        } else {
            std::string text;
            size_t redirect_pos = 0;
            for (size_t i = 1; i < args.size(); i++) {
                if (args[i] == ">") {
                    redirect_pos = i;
                    break;
                }
                if (i > 1) text += " ";
                text += args[i];
            }
            
            if (redirect_pos > 0 && redirect_pos + 1 < args.size()) {
                // 写入文件
                std::string filename = args[redirect_pos + 1];
                int fd = kernel_.get_file_system().open_file(filename);
                if (fd >= 0) {
                    text += "\n";
                    kernel_.get_file_system().write_file(fd, text.c_str(), text.size());
                    kernel_.get_file_system().close_file(fd);
                } else {
                    std::cerr << "Failed to open file: " << filename << "\n";
                }
            } else {
                // 输出到屏幕（stderr）
                std::cerr << text << "\n";
            }
        }
    } else if (cmd == "fsinfo") {
        kernel_.get_file_system().print_superblock();
    
    } else if (cmd == "exit") {
        running_ = false;
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
    }
}

void Shell::execute_script(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open script file '" << filename << "'\n";
        return;
    }

    std::cerr << "Executing script: " << filename << "\n";
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::cerr << ">>> " << line << "\n";
        auto args = parse_command(line);
        if (!args.empty()) {
            execute_command(args);
        }
    }
    
    file.close();
    std::cerr << "Script execution completed.\n";
}
