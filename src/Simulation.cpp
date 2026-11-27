#include "../include/Simulation.h"

#include <limits>
#include <stdexcept>
#include <utility>

const Simulation::Process& Simulation::addProcess(std::string name, int burstTime) {
    if (burstTime <= 0) {
        throw std::invalid_argument("Burst time must be positive");
    }
    if (processes_.size() == std::numeric_limits<int>::max()) {
        throw std::overflow_error("No process IDs remain");
    }
    const int pid = static_cast<int>(processes_.size()) + 1;
    processes_.push_back({PCB(pid, std::move(name), burstTime, 0, -1, clock_.now())});
    try {
        scheduler_.receivePCB(processes_.back().pcb);
    } catch (...) {
        processes_.pop_back();
        throw;
    }
    return processes_.back();
}

Simulation::Tick Simulation::step() {
    if (clock_.now() == std::numeric_limits<int>::max()) {
        throw std::overflow_error("Simulation clock overflow");
    }

    // 在节拍开始时处理高级队列到达；抢占保留原级别并保存上下文。
    if (!cpu_.isIdle() && scheduler_.ShouldPreempt(*cpu_.getCurrentProcess())) {
        scheduler_.Preempted(*cpu_.release());
    }
    const bool dispatched = scheduler_.ProcessShifter();
    if (cpu_.isIdle()) {
        clock_.advance();
        return {nullptr, CPU::TickResult::Idle, 0, false, 0};
    }

    // PID 从 1 递增，已结束进程仍保留在进程表中。
    Process& process = processes_[cpu_.getCurrentProcess()->getPid() - 1];
    PCB& pcb = process.pcb;
    process.queueLevel = scheduler_.getLevelOf(pcb.getPid());
    const auto previousR0 = cpu_.getContext().registers[0];
    const Tick tick{&process, cpu_.tick(), process.queueLevel, dispatched, previousR0};
    // 与演示一致，用 R0 记录已完成的工作量，便于验证上下文切换。
    auto context = cpu_.getContext();
    ++context.registers[0];
    cpu_.setContext(context);

    for (auto& waiting : processes_) {
        if (waiting.pcb.isReady()) {
            waiting.pcb.updateWaitTime(1);
        }
    }
    clock_.advance();

    if (tick.result == CPU::TickResult::Finished) {
        pcb.terminate(clock_.now());
        cpu_.release();
        scheduler_.ProcessFinished(pcb);
    } else if (tick.result == CPU::TickResult::TimeSliceExpired) {
        cpu_.release();
        scheduler_.TimeSliceExpired(pcb);
        process.queueLevel = scheduler_.getLevelOf(pcb.getPid());
    }
    return tick;
}
