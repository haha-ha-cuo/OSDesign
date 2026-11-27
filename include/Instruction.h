//
// Created by YuHao Shi on 2026/11/27.
//

#ifndef OSDESIGN_INSTRUCTION_H
#define OSDESIGN_INSTRUCTION_H

class Instruction {
public:
    enum InstructionType { COMPUTE, IO, READ, Yield, Exit };
};

#endif //OSDESIGN_INSTRUCTION_H
