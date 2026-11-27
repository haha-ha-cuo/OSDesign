#include "../include/Core.h"
#include "PCB.h"
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

Core::State Core::createProcess(PCB*& pcb) {
    for (PCB& slot : dram.getPCBList()) {
        if (slot.state == PCB::BLANK) {
            slot = PCB{};
            slot.state = PCB::CREATING;
            slot.pid = allocatePid();
            pcb = &slot;
            return SUCCESS;
        }
    }
    pcb = nullptr;
    return FAILURE;
}

Core::State Core::createChildProcess(PCB* pcb) {
    if (pcb == nullptr) {
        return FAILURE;
    }
    PCB* child = nullptr;
    if (createProcess(child) == FAILURE) {
        return FAILURE;
    }
    child->ppid = pcb->pid;
    pcb->childrens.push_back(child);
    return SUCCESS;
}

Core::State Core::loadfile(Process* process, const std::string& filename) {
    if (process == nullptr) {
        return FAILURE;
    }

    std::ifstream fileStream(filename);
    if (!fileStream.is_open()) {
        return FAILURE;
    }

    std::vector<Instruction> programs;
    std::string line;
    try {
        while (std::getline(fileStream, line)) {
            if (line.find_first_not_of(" \t\r\n\v\f") == std::string::npos) {
                continue;
            }
            programs.push_back(Instruction::parseInstruction(line));
        }
    } catch (const std::invalid_argument&) {
        return FAILURE;
    }

    if (!fileStream.eof() || fileStream.bad() || programs.empty()) {
        return FAILURE;
    }

    // 整个文件通过检查后再替换，避免失败时破坏已有程序。
    process->programs = std::move(programs);
    return SUCCESS;
}
