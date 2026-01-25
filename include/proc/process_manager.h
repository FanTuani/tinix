#pragma once
#include "process.h"
#include "instruction.h"
#include "mem/memory_manager.h"
#include <map>
#include <queue>
#include <string>
#include <memory>

class Program;

class ProcessManager {
public:
    ProcessManager(size_t num_frames, size_t page_size);
    
    int create_process(int total_time = 10);
    int create_process_from_file(const std::string& filename);
    int create_process_with_program(std::shared_ptr<Program> program);
    
    void terminate_process(int pid);
    void dump_processes() const;
    void tick();
    void run_process(int pid);
    void block_process(int pid, int duration);
    void wakeup_process(int pid);
    
    MemoryManager& get_memory_manager() { return memory_manager_; }

private:
    std::map<int, PCB> processes_;
    std::queue<int> ready_queue_;
    int next_pid_ = 1;
    int next_tick_ = 0;
    int cur_pid_ = -1;
    
    MemoryManager memory_manager_;
    
    void schedule();
    void check_blocked_processes();
    void execute_instruction(PCB& pcb, const Instruction& inst);
};
