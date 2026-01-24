#pragma once
#include "instruction.h"
#include <vector>
#include <string>
#include <memory>

class Program {
public:
    static std::shared_ptr<Program> load_from_file(const std::string& filename);
    static std::shared_ptr<Program> create_default(int length);
    static std::shared_ptr<Program> create_compute_only(int length);
    
    const Instruction& get_instruction(size_t pc) const;
    size_t size() const { return instructions_.size(); }
    
private:
    std::vector<Instruction> instructions_;
    
    Program() = default;
    static std::vector<Instruction> parse_file(const std::string& filename);
};
