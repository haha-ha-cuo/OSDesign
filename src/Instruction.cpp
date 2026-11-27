#include "../include/Instruction.h"

#include <sstream>
#include <stdexcept>

Instruction Instruction::parseInstruction(const std::string& line) {
    std::istringstream input(line);
    std::string operation;
    input >> operation;

    Instruction instruction{};
    if (operation == "COMPUTE") {
        instruction.type = COMPUTE;
    } else if (operation == "IO") {
        instruction.type = IO;
    } else if (operation == "READ") {
        instruction.type = READ;
    } else if (operation == "YIELD") {
        instruction.type = Yield;
    } else if (operation == "EXIT") {
        instruction.type = Exit;
    } else {
        throw std::invalid_argument("未知指令：" + operation);
    }

    if (instruction.type != Yield) {
        if (!(input >> instruction.argument)) {
            throw std::invalid_argument("指令需要一个整数参数：" + operation);
        }
        if (instruction.type != Exit && instruction.argument <= 0) {
            throw std::invalid_argument("执行时间必须大于零：" + operation);
        }
    }

    std::string extra;
    if (input >> extra) {
        throw std::invalid_argument("指令包含多余参数：" + operation);
    }

    return instruction;
}
