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

// ====================================================================
// 以下为本地改写版（已整体注释，仅作备份，不参与编译）
// 来源分支：backup/wip-local-mlfq-ps:include/MLFQ.h
// 与当前主线版 MLFQ 的 API 不兼容；需要恢复时，把下面内容复制到文件顶部替换。
// ====================================================================
//
// Created by WCY on 2026/11/20.
//

// #ifndef OSDESIGN_MLFQ_H
// #define OSDESIGN_MLFQ_H

// #include "CLOCK.h"
// #include "CPU.h"
// #include "PCB.h"

// #include <deque>
// #include <memory>
// #include <string>
// #include <unordered_map>
// #include <vector>

// 多级反馈队列（Multilevel Feedback Queue，MLFQ）。
//
// 规则：
// 1. 新进程进入第 0 级队列（最高优先级、最短时间片）；
// 2. 每个队列内部按先来先服务排队，进程获得该级时间片
//    baseSlice * sliceGrowth^level，默认 1/2/4/8/16 tick；
// 3. 时间片用完仍未结束：降一级放入队尾，时间片随之变长；
// 4. 被高级别队列抢占：回到原队列队尾，不降级；
// 5. 高级别队列非空时，可以抢占低级别队列中正在运行的进程；
// 6. 可选周期性提升：每 boostInterval 个 tick 把等待中的低级别进程送回第 0 级，
//    防止长作业在低级别队列里饿死。
// class MLFQ {
// public:
//     struct Options {
//         int levels; // 队列级数，至少 1
//         int baseSlice; // 第 0 级队列的时间片
//         int sliceGrowth; // 每降一级时间片的放大倍数，至少 1
//         bool preemptive; // 高级别队列非空时是否抢占
//         int boostInterval; // 0 表示关闭周期性提升，否则每 N 个 tick 提升一次

//         Options();
//     };

// 甘特图中的一个连续执行片段，queueLevel 记录运行时所在队列。
//     struct TimelineEntry {
//         int pid;
//         std::string name;
//         int startTime;
//         int endTime;
//         int queueLevel;

//         [[nodiscard]] int duration() const { return endTime - startTime; }
//     };

//     struct Metrics {
//         double averageTurnaroundTime = 0.0;
//         double averageWaitingTime = 0.0;
//         double averageResponseTime = 0.0;
//         double cpuUtilization = 0.0; // 百分比
//         double throughput = 0.0; // 每个 tick 完成的作业数
//         int totalTicks = 0;
//         int busyTicks = 0;
//         int finishedProcesses = 0;
//     };

// CPU 必须绑定到同一个 clock，且此后只由本调度器装载进程。
//     MLFQ(CLOCK& clock, CPU& cpu, Options options = {});
//     MLFQ(const MLFQ&) = delete;
//     MLFQ& operator=(const MLFQ&) = delete;

// 登记作业；PID 必须唯一，服务时间必须为正。到达时刻前进程保持 New。
//     PCB& addProcess(int pid, std::string name, int burstTime, int priority = 0,
//                     int arrivalTime = 0);

// 推进一个系统 tick：到达 -> 抢占/调度 -> 执行 -> 等待计时 -> 时钟推进 -> 处理结果。
//     void step();

// 反复调用 step() 直到全部进程结束；超过 maxTicks 抛出 std::overflow_error。
//     void run(int maxTicks = 1'000'000);

//     [[nodiscard]] bool isFinished() const;
//     [[nodiscard]] const std::vector<std::unique_ptr<PCB>>& getProcesses() const {
//         return processes_;
//     }
//     [[nodiscard]] const std::vector<TimelineEntry>& getTimeline() const { return timeline_; }
//     [[nodiscard]] Metrics getMetrics() const;
//     [[nodiscard]] const Options& getOptions() const { return options_; }
//     [[nodiscard]] int getLevels() const { return options_.levels; }
// 第 level 级队列的时间片。
//     [[nodiscard]] int getTimeSlice(int level) const;
// 第 level 级队列中等待的进程数（不含正在运行的进程）。
//     [[nodiscard]] int getQueueSize(int level) const;
// 进程当前所在队列级别；未登记或已结束返回 -1。
//     [[nodiscard]] int getLevelOf(int pid) const;

// private:
//     void admitArrivals();
//     PCB* pickNext();
//     [[nodiscard]] bool shouldPreempt(const PCB& running) const;
//     void onTimeSliceExpired(PCB& process);
//     void onPreempted(PCB& process);
//     void onFinished(PCB& process);
//     void onTickEnd();
//     void enqueue(PCB& process, int level);
//     [[nodiscard]] int levelOf(const PCB& process) const;
//     void validateLevel(int level) const;
//     void recordExecution(const PCB& running, int now);

//     CLOCK& clock_;
//     CPU& cpu_;
//     Options options_;
//     std::vector<std::unique_ptr<PCB>> processes_;
//     std::vector<std::deque<PCB*>> queues_;
//     std::unordered_map<int, int> levelByPid_;
//     std::vector<TimelineEntry> timeline_;
//     int busyTicks_ = 0;
//     int totalTicks_ = 0;
//     int nextBoostTick_ = 0;
//     PCB* running_ = nullptr;
// };

// #endif //OSDESIGN_MLFQ_H
