#pragma once
#include <cstdint>
#include <string>

enum class OpType {
    Compute,
    MemRead,
    MemWrite,
    FileOpen,
    FileClose,
    FileRead,
    FileWrite,
    DevRequest,
    DevRelease,
    Sleep
};

struct Instruction {
    OpType type;
    uint64_t arg1 = 0;
    uint64_t arg2 = 0;
    std::string str_arg;
    
    Instruction(OpType t, uint64_t a1 = 0, uint64_t a2 = 0, const std::string& s = "")
        : type(t), arg1(a1), arg2(a2), str_arg(s) {}
};
