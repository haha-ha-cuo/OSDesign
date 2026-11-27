//
// Created by YuHao Shi on 2026/11/27.
//

#include "../include/Core.h"
#include "PCB.h"
#include <vector>

Core::Satate Core::createProcess(PCB* pcb) {
    std::vector<PCB> PCBList = dram.getPCBList();
    for (PCB pcb1 : PCBList) {
        if (pcb1.state == PCB::BLANK) {
            pcb1.state = PCB::CREATING;
            pcb1.pid = allocatePid();
            pcb = new PCB(pcb1);
            return Core::SUCCESS;
        }
    }
    return Core::FAILURE;
}

Core::Satate Core::createChildProcess(PCB* pcb) {
    std::vector<PCB> PCBList = dram.getPCBList();
    for (PCB pcb1 : PCBList) {
        if (pcb1.state == PCB::BLANK) {
            pcb1.state = PCB::CREATING;
            pcb1.pid = allocatePid();
            pcb1.ppid = pcb->pid;
            pcb->childrens.push_back(&pcb1);
            return Core::SUCCESS;
        }
    }
    return Core::FAILURE;
}
