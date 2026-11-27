//
// Created by YuHao Shi on 2026/11/27.
//

#ifndef OSDESIGN_PCB_H
#define OSDESIGN_PCB_H

#include "FCB.h"
#include "IO.h"
#include <string>
#include <vector>

class PCB {
public:
    enum State { READY, RUNNING, BLOCK, CREATING, END, BLANK };
    State state = BLANK;
    int pid;
    std::string name;
    int ppid; //父进程进程号
    int priority;
    int waitCPUTime;
    int CPUTime;
    int remainingTime;
    std::string incident;
    std::vector<PCB*> childrens;
    FCB* fcb;
    IO* io;
    PCB* next;
};

#endif //OSDESIGN_PCB_H
