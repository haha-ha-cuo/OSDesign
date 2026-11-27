//
// Created by YuHao Shi on 2026/11/27.
//

#ifndef OSDESIGN_CPU_H
#define OSDESIGN_CPU_H
#include "PCB.h"

class CPU {
public:
    int id;
    PCB* pcb;
    enum State { ERROR, IO, END };
    State execute();
    void contextShifter(){pcb = pcb->next;};
};

#endif //OSDESIGN_CPU_H
