//
// Created by YuHao Shi on 2026/11/13.
//

#ifndef OSDESIGN_MLFQ_H
#define OSDESIGN_MLFQ_H

#include "CPU.h"
#include "PCB.h"

#include <array>
#include <queue>

// 五级反馈队列：1 级优先级最高，同级按 FCFS 派发。
// 队列只保存 PCB 指针，PCB 本体由进程表持有，避免复制进程状态。
class MLFQ {
public:
    static constexpr int kLevels = 5;
    static constexpr int kDefaultTimePiece = 16;

    // cpu 必须比 MLFQ 活得久；timePiece 是每个队列使用的时间片长度。
    explicit MLFQ(CPU& cpu, int timePiece = kDefaultTimePiece);

    std::queue<PCB*> Ready_processes1;
    std::queue<PCB*> Ready_processes2;
    std::queue<PCB*> Ready_processes3;
    std::queue<PCB*> Ready_processes4;
    std::queue<PCB*> Ready_processes5;
    int timePiece;

    // 就绪进程入队：level 取 1..5，默认进入最高优先级队列。
    // New 会自动转成 Ready；已终止、正在运行或阻塞的进程不能入队。
    void receivePCB(PCB& pcb, int level = 1);

    // CPU 空闲时，从最高优先级非空队列取出队首进程并装载。
    // 返回是否发生派发；CPU 忙或所有队列为空时返回 false。
    // 进程时间片耗尽并 release() 后，由调用方用 receivePCB(pcb, level + 1) 降级。
    bool ProcessShifter();

private:
    std::array<std::queue<PCB*>*, kLevels> queues() noexcept;

    CPU& cpu_;
};

#endif // OSDESIGN_MLFQ_H
