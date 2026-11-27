//
// Created by YuHao Shi on 2026/11/27.
//

#ifndef OSDESIGN_OS_H
#define OSDESIGN_OS_H
#include "Dram.h"
#include "PCB.h"

class Core {
public:
    enum Satate {
        SUCCESS,
        FAILURE,
    };
    Satate createProcess(PCB* pcb);
    Satate createChildProcess(PCB* pcb);
    Dram dram;
    int pid_ = 1;
    int allocatePid() { return pid_++; }
};

#endif //OSDESIGN_OS_H
