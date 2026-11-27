#include "Core.h"

#include <iostream>
#include <stdexcept>

int main() {
    try {
        Core core;
        for (PCB& slot : core.dram.getPCBList()) {
            PCB* pcb = nullptr;
            if (core.createProcess(pcb) != Core::SUCCESS || pcb != &slot ||
                slot.state != PCB::CREATING || slot.CPUTime != 0 || slot.programCounter != 0) {
                throw std::runtime_error("创建未返回真实的主存槽位");
            }
        }
        PCB* full = nullptr;
        if (core.createProcess(full) != Core::FAILURE || full != nullptr) {
            throw std::runtime_error("PCB 表满时未正确返回失败");
        }
        Core family;
        PCB* parent = nullptr;
        family.createProcess(parent);
        if (family.createChildProcess(nullptr) != Core::FAILURE ||
            family.createChildProcess(parent) != Core::SUCCESS || parent->childrens.size() != 1) {
            throw std::runtime_error("创建子进程失败");
        }
        PCB* child = parent->childrens.front();
        if (child != &family.dram.getPCBList()[1] || child->ppid != parent->pid ||
            child->state != PCB::CREATING) {
            throw std::runtime_error("子进程未关联有效的主存槽位");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
