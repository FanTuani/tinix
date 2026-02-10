#include "proc/program.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <limits>

std::shared_ptr<Program> Program::load_from_file(const std::string& filename) {
    auto instructions = parse_file(filename);
    if (instructions.empty()) {
        return nullptr;
    }
    
    auto prog = std::shared_ptr<Program>(new Program());
    prog->instructions_ = std::move(instructions);
    return prog;
}

std::shared_ptr<Program> Program::create_default(int length) {
    return create_compute_only(length);
}

std::shared_ptr<Program> Program::create_compute_only(int length) {
    auto prog = std::shared_ptr<Program>(new Program());
    for (int i = 0; i < length; i++) {
        prog->instructions_.emplace_back(OpType::Compute);
    }
    return prog;
}

const Instruction& Program::get_instruction(size_t pc) const {
    return instructions_[pc];
}

std::vector<Instruction> Program::parse_file(const std::string& filename) {
    std::vector<Instruction> instructions;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return instructions;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream iss(line);
        std::string op;
        iss >> op;
        
        if (op == "C" || op == "COMPUTE") {
            instructions.emplace_back(OpType::Compute);
        } else if (op == "R" || op == "MEMREAD") {
            uint64_t addr;
            iss >> std::setbase(0) >> addr;
            instructions.emplace_back(OpType::MemRead, addr);
        } else if (op == "W" || op == "MEMWRITE") {
            uint64_t addr;
            iss >> std::setbase(0) >> addr;
            instructions.emplace_back(OpType::MemWrite, addr);
        } else if (op == "FO" || op == "FILEOPEN") {
            std::string first;
            if (!(iss >> first)) {
                std::cerr << "Invalid FO syntax (missing arguments) in "
                          << filename << std::endl;
                continue;
            }

            std::string second;
            if (iss >> second) {
                uint64_t fd = 0;
                std::istringstream fd_iss(first);
                fd_iss >> std::setbase(0) >> fd;
                if (fd_iss.fail()) {
                    std::cerr << "Invalid FO syntax (bad fd): " << line
                              << std::endl;
                    continue;
                }
                instructions.emplace_back(OpType::FileOpen, fd, 0, second);
            } else {
                instructions.emplace_back(OpType::FileOpen,
                                          std::numeric_limits<uint64_t>::max(),
                                          0,
                                          first);
            }
        } else if (op == "FC" || op == "FILECLOSE") {
            uint64_t fd;
            iss >> std::setbase(0) >> fd;
            instructions.emplace_back(OpType::FileClose, fd);
        } else if (op == "FR" || op == "FILEREAD") {
            uint64_t fd, size;
            iss >> std::setbase(0) >> fd >> std::setbase(0) >> size;
            instructions.emplace_back(OpType::FileRead, fd, size);
        } else if (op == "FW" || op == "FILEWRITE") {
            uint64_t fd, size;
            iss >> std::setbase(0) >> fd >> std::setbase(0) >> size;
            instructions.emplace_back(OpType::FileWrite, fd, size);
        } else if (op == "DR" || op == "DEVREQ") {
            uint64_t dev;
            iss >> std::setbase(0) >> dev;
            instructions.emplace_back(OpType::DevRequest, dev);
        } else if (op == "DD" || op == "DEVREL") {
            uint64_t dev;
            iss >> std::setbase(0) >> dev;
            instructions.emplace_back(OpType::DevRelease, dev);
        } else if (op == "S" || op == "SLEEP") {
            uint64_t duration;
            iss >> std::setbase(0) >> duration;
            instructions.emplace_back(OpType::Sleep, duration);
        }
    }
    
    std::cerr << "Loaded " << instructions.size() << " instructions from " << filename << std::endl;
    return instructions;
}
