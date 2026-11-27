#ifndef OSDESIGN_PCB_H
#define OSDESIGN_PCB_H

#include "FCB.h"
#include "IO.h"
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

class Process;

class PCB {
public:
    enum State { READY, RUNNING, BLOCK, CREATING, END, BLANK };
    State state = BLANK;
    int pid;
    std::string name;
    int ppid; //父进程进程号
    int priority;
    int waitCPUTime;
    int CPUTime = 0;
    int remainingTime;
    std::size_t programCounter = 0; // 当前指令下标
    int instructionRemainingTicks = 0; // 当前 COMPUTE 指令剩余时钟数
    int ioRemainingTicks = 0; // IO/READ 请求的剩余服务时钟数，由设备模块推进
    int exitCode = 0;
    std::string incident;
    std::vector<PCB*> childrens;
    FCB* fcb = nullptr;
    IO* io = nullptr;
    PCB* next = nullptr;
    Process* process = nullptr;
};

#endif //OSDESIGN_PCB_H
