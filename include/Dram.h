#ifndef OSDESIGN_MEMORY_H
#define OSDESIGN_MEMORY_H
#include "PCB.h"
#include <array>
#include <span>

class Dram {
private:
    std::array<PCB, 16> PCBList{};

public:
    // 槽位地址在 Dram 生命周期内保持稳定，调用方不可改变表的大小。
    std::span<PCB> getPCBList() { return PCBList; }
};

#endif //OSDESIGN_MEMORY_H
