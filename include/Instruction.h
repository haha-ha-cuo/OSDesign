#ifndef OSDESIGN_INSTRUCTION_H
#define OSDESIGN_INSTRUCTION_H

#include <string>

class Instruction {
public:
    enum InstructionType { COMPUTE, IO, READ, Yield, Exit };

    InstructionType type;
    int argument = 0; // COMPUTE、IO、READ 为模拟时钟数，Exit 为退出码

    /**
     * 解析大写指令：COMPUTE/IO/READ 后接正整数，YIELD 无参数，EXIT 后接整数退出码。
     * 格式或参数非法时抛出 std::invalid_argument。
     */
    static Instruction parseInstruction(const std::string& line);
};

#endif //OSDESIGN_INSTRUCTION_H
