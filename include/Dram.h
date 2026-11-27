//
// Created by YuHao Shi on 2026/11/27.
//

#ifndef OSDESIGN_MEMORY_H
#define OSDESIGN_MEMORY_H
#include "PCB.h"
#include <vector>

class Dram {
private:
    std::vector<PCB> PCBList;

public:
    std::vector<PCB> getPCBList() { return PCBList; };
};

#endif //OSDESIGN_MEMORY_H
