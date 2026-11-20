//
// Created by YuHao Shi on 2026/11/13.
//

#ifndef OSDESIGN_MLFQ_H
#define OSDESIGN_MLFQ_H

#include "CPU.h"
#include "PCB.h"

#include <array>
#include <queue>
#include <unordered_map>

// 五级反馈队列（MLFQ）：1 级优先级最高，5 级最低，同级按 FCFS 派发。
// 队列只保存 PCB 指针，PCB 本体由进程表持有，避免复制进程状态。
class MLFQ {
public:
    static constexpr int kLevels = 5;
    static constexpr int kDefaultTimePiece = 16;

    // cpu 必须比 MLFQ 活得久；所有队列统一使用时间片 timePiece。
    explicit MLFQ(CPU& cpu, int timePiece = kDefaultTimePiece);

    std::queue<PCB*> Ready_processes1;
    std::queue<PCB*> Ready_processes2;
    std::queue<PCB*> Ready_processes3;
    std::queue<PCB*> Ready_processes4;
    std::queue<PCB*> Ready_processes5;
    int timePiece;

    // 就绪进程入队：level 取 1..5，默认进入最高优先级队列。
    // New 会自动转成 Ready；已终止、正在运行、已在本类队列中或已完成的进程不能入队。
    void receivePCB(PCB& pcb, int level = 1);

    // CPU 空闲时，从最高优先级非空队列取出队首进程并装载。
    // 返回是否发生派发；CPU 忙或所有队列为空时返回 false。
    // 派发成功时按 timePiece 重新设置时间片，并记录该进程所在级别。
    bool ProcessShifter();

    // 时间片耗尽：调用方 release() 之后调用，自动降一级并重新入队；
    // 已在最低级时保持原级别。要求 pcb 是最近一次 ProcessShifter() 派发的进程。
    void TimeSliceExpired(PCB& pcb);

    // 被抢占：调用方 release() 之后调用，回到原级别队尾，不降级。
    void Preempted(PCB& pcb);

    // 进程终止后调用，清除派发记录与级别信息；PCB 销毁前应调用。
    void ProcessFinished(PCB& pcb);

    // 是否有更高级别队列等待，供调用方决定是否抢占 running。
    [[nodiscard]] bool ShouldPreempt(const PCB& running) const;

    // 周期性提升：把所有等待进程按级别从高到低依次送回 1 级队列，防止饥饿。
    void BoostQueues();

    // 进程当前所在级别（排队中或正在运行）；未登记或已结束返回 -1。
    [[nodiscard]] int getLevelOf(int pid) const;
    // 第 level 级队列中等待的进程数（不含正在运行的进程）。
    [[nodiscard]] int getQueueSize(int level) const;
    // 所有队列中等待的进程总数。
    [[nodiscard]] int getReadyCount() const;

private:
    std::array<std::queue<PCB*>*, kLevels> queues() noexcept;
    std::array<const std::queue<PCB*>*, kLevels> queues() const noexcept;

    void enqueue(PCB& pcb, int level);
    // 清除某个进程的派发记录（重新入队、终止或再次入队时调用）。
    void clearDispatch(int pid);

    CPU& cpu_;
    // 仅记录排队中的进程；派发后从映射中移除，改由 dispatchedPid_ 记录。
    std::unordered_map<int, int> levelByPid_;
    int dispatchedPid_ = -1;
    int dispatchedLevel_ = 0;
};

#endif // OSDESIGN_MLFQ_H
