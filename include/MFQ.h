//
// Created by YuHao Shi on 2026/11/13.
//

#ifndef OSDESIGN_FCFS_H
#define OSDESIGN_FCFS_H
#include "../include/PCB.h"
#include <queue>

class MFQ {
public:
    MFQ();
    std::queue<PCB> Ready_processes1;
    std::queue<PCB> Ready_processes2;
    std::queue<PCB> Ready_processes3;
    std::queue<PCB> Ready_processes4;
    std::queue<PCB> Ready_processes5;
    int timePiece;

private:
    void FCFS();
    void ContextShifter();
    void ProcessShifter();
};

#endif //OSDESIGN_FCFS_H
