#ifndef OSDESIGN_SIMULATION_H
#define OSDESIGN_SIMULATION_H

#include "MLFQ.h"

#include <cstdint>
#include <deque>
#include <string>

class Simulation {
public:
    struct Process {
        PCB pcb;
        int queueLevel = 1;
    };

    struct Tick {
        const Process* process;
        CPU::TickResult result;
        int queueLevel; // 本拍执行前的队列等级。
        bool dispatched;
        std::uint64_t previousR0;
    };

    Simulation() = default;
    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    const Process& addProcess(std::string name, int burstTime);
    Tick step();

    int time() const noexcept { return clock_.now(); }
    const CPU& cpu() const noexcept { return cpu_; }
    const std::deque<Process>& processes() const noexcept { return processes_; }

private:
    CLOCK clock_;
    // 队尾添加进程时保留已有 PCB 地址，CPU 和 MLFQ 都借用这些地址。
    std::deque<Process> processes_;
    CPU cpu_{0, clock_};
    MLFQ scheduler_{cpu_, 4};
};

#endif // OSDESIGN_SIMULATION_H
