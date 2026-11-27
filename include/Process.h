#ifndef OSDESIGN_PROCESS_H
#define OSDESIGN_PROCESS_H

#include "Instruction.h"
#include "PCB.h"
#include <vector>

class Process {
    friend class Core;

public:
    PCB* pcb = nullptr;
    std::vector<Instruction> programs;
    Process();
};

#endif //OSDESIGN_PROCESS_H
