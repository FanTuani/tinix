#include "proc/program.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <limits>

namespace {
bool parse_u64_token(const std::string& token, uint64_t& value) {
    std::istringstream iss(token);
    iss >> std::setbase(0) >> value;
    return !iss.fail() && iss.eof();
}
}  // namespace

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
    size_t line_no = 0;
    while (std::getline(file, line)) {
        ++line_no;
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream iss(line);
        std::string op;
        iss >> op;
        
        if (op == "C" || op == "COMPUTE") {
            instructions.emplace_back(OpType::Compute);
        } else if (op == "R" || op == "MEMREAD") {
            std::string token;
            uint64_t addr = 0;
            if (!(iss >> token) || !parse_u64_token(token, addr)) {
                std::cerr << "Invalid MemRead operand at " << filename << ":"
                          << line_no << std::endl;
                return {};
            }
            instructions.emplace_back(OpType::MemRead, addr);
        } else if (op == "W" || op == "MEMWRITE") {
            std::string token;
            uint64_t addr = 0;
            if (!(iss >> token) || !parse_u64_token(token, addr)) {
                std::cerr << "Invalid MemWrite operand at " << filename << ":"
                          << line_no << std::endl;
                return {};
            }
            instructions.emplace_back(OpType::MemWrite, addr);
        } else if (op == "FO" || op == "FILEOPEN") {
            std::string first;
            if (!(iss >> first)) {
                std::cerr << "Invalid FO syntax (missing arguments) in "
                          << filename << ":" << line_no << std::endl;
                return {};
            }

            std::string second;
            if (iss >> second) {
                uint64_t fd = 0;
                if (!parse_u64_token(first, fd)) {
                    std::cerr << "Invalid FO syntax (bad fd): " << line
                              << std::endl;
                    return {};
                }
                instructions.emplace_back(OpType::FileOpen, fd, 0, second);
            } else {
                instructions.emplace_back(OpType::FileOpen,
                                          std::numeric_limits<uint64_t>::max(),
                                          0,
                                          first);
            }
        } else if (op == "FC" || op == "FILECLOSE") {
            std::string token;
            uint64_t fd = 0;
            if (!(iss >> token) || !parse_u64_token(token, fd)) {
                std::cerr << "Invalid FileClose operand at " << filename << ":"
                          << line_no << std::endl;
                return {};
            }
            instructions.emplace_back(OpType::FileClose, fd);
        } else if (op == "FR" || op == "FILEREAD") {
            std::string fd_token;
            std::string size_token;
            uint64_t fd = 0;
            uint64_t size = 0;
            if (!(iss >> fd_token >> size_token) ||
                !parse_u64_token(fd_token, fd) ||
                !parse_u64_token(size_token, size)) {
                std::cerr << "Invalid FileRead operands at " << filename << ":"
                          << line_no << std::endl;
                return {};
            }
            instructions.emplace_back(OpType::FileRead, fd, size);
        } else if (op == "FW" || op == "FILEWRITE") {
            std::string fd_token;
            std::string size_token;
            uint64_t fd = 0;
            uint64_t size = 0;
            if (!(iss >> fd_token >> size_token) ||
                !parse_u64_token(fd_token, fd) ||
                !parse_u64_token(size_token, size)) {
                std::cerr << "Invalid FileWrite operands at " << filename
                          << ":" << line_no << std::endl;
                return {};
            }
            instructions.emplace_back(OpType::FileWrite, fd, size);
        } else if (op == "DR" || op == "DEVREQ") {
            std::string token;
            uint64_t dev = 0;
            if (!(iss >> token) || !parse_u64_token(token, dev)) {
                std::cerr << "Invalid DevRequest operand at " << filename
                          << ":" << line_no << std::endl;
                return {};
            }
            instructions.emplace_back(OpType::DevRequest, dev);
        } else if (op == "DD" || op == "DEVREL") {
            std::string token;
            uint64_t dev = 0;
            if (!(iss >> token) || !parse_u64_token(token, dev)) {
                std::cerr << "Invalid DevRelease operand at " << filename
                          << ":" << line_no << std::endl;
                return {};
            }
            instructions.emplace_back(OpType::DevRelease, dev);
        } else if (op == "S" || op == "SLEEP") {
            std::string token;
            uint64_t duration = 0;
            if (!(iss >> token) || !parse_u64_token(token, duration)) {
                std::cerr << "Invalid Sleep operand at " << filename << ":"
                          << line_no << std::endl;
                return {};
            }
            instructions.emplace_back(OpType::Sleep, duration);
        } else {
            std::cerr << "Unknown opcode at " << filename << ":" << line_no
                      << ": " << op << std::endl;
            return {};
        }
    }
    
    std::cerr << "Loaded " << instructions.size() << " instructions from " << filename << std::endl;
    return instructions;
}
