//
// Created by WCY on 2026/11/20.
//

#ifndef OSDESIGN_PS_H
#define OSDESIGN_PS_H

#include "CLOCK.h"
#include "CPU.h"
#include "PCB.h"

#include <memory>
#include <string>
#include <vector>

// 优先级调度（Priority Scheduling）。
//
// 规则：
// 1. 总是选择就绪队列中优先级数值最小的进程，数值越小优先级越高；
// 2. 优先级相同按到达时间先来先服务，到达时间也相同时按 PID 升序，保证结果可复现；
// 3. 抢占式：新进程到达时若优先级更高，立即抢占当前进程；
//    非抢占式：新进程只入就绪队列，等当前进程结束才重新选择；
// 4. 可选老化：每 agingInterval 个 tick 让所有等待中的就绪进程提升一级
//    （优先级减 1，下限 lowestPriority），防止低优先级进程长期饥饿。
class PS {
public:
    struct Options {
        bool preemptive; // true：抢占式；false：非抢占式
        int agingInterval; // 0 表示关闭老化，否则每 N 个 tick 提升一级
        int lowestPriority; // 老化后的优先级下限

        Options();
    };

    struct TimelineEntry {
        int pid;
        std::string name;
        int startTime;
        int endTime;

        [[nodiscard]] int duration() const { return endTime - startTime; }
    };

    struct Metrics {
        double averageTurnaroundTime = 0.0;
        double averageWaitingTime = 0.0;
        double averageResponseTime = 0.0;
        double cpuUtilization = 0.0; // 百分比
        double throughput = 0.0; // 每个 tick 完成的作业数
        int totalTicks = 0;
        int busyTicks = 0;
        int finishedProcesses = 0;
    };

    // CPU 必须绑定到同一个 clock，且此后只由本调度器装载进程。
    PS(CLOCK& clock, CPU& cpu, Options options = {});
    PS(const PS&) = delete;
    PS& operator=(const PS&) = delete;

    // 登记作业；PID 必须唯一，服务时间必须为正。到达时刻前进程保持 New。
    PCB& addProcess(int pid, std::string name, int burstTime, int priority = 0,
                    int arrivalTime = 0);

    // 推进一个系统 tick：到达 -> 抢占/调度 -> 执行 -> 等待计时 -> 时钟推进 -> 处理结果。
    void step();

    // 反复调用 step() 直到全部进程结束；超过 maxTicks 抛出 std::overflow_error。
    void run(int maxTicks = 1'000'000);

    [[nodiscard]] bool isFinished() const;
    [[nodiscard]] const std::vector<std::unique_ptr<PCB>>& getProcesses() const {
        return processes_;
    }
    [[nodiscard]] const std::vector<TimelineEntry>& getTimeline() const { return timeline_; }
    [[nodiscard]] Metrics getMetrics() const;
    [[nodiscard]] const Options& getOptions() const { return options_; }

private:
    void admitArrivals();
    PCB* pickNext();
    [[nodiscard]] bool shouldPreempt(const PCB& running) const;
    void onTickEnd();
    void recordExecution(const PCB& running, int now);
    [[nodiscard]] static bool higherPriority(const PCB& candidate, const PCB& current);

    CLOCK& clock_;
    CPU& cpu_;
    Options options_;
    std::vector<std::unique_ptr<PCB>> processes_;
    std::vector<TimelineEntry> timeline_;
    int busyTicks_ = 0;
    int totalTicks_ = 0;
    int nextAgingTick_ = 0;
    PCB* running_ = nullptr;
};

#endif //OSDESIGN_PS_H
