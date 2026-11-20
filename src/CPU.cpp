//
// Created by YuHao Shi on 2026/11/13.
//

#include "../include/CPU.h"

#include <limits>
#include <stdexcept>

CPU::CPU(int id, const CLOCK& clock) : id_(id), clock_(clock), nextTickTime_(clock.now()) {
    if (id < 0) {
        throw std::invalid_argument("CPU ID cannot be negative");
    }
}

void CPU::dispatch(PCB& process) {
    if (!isIdle()) {
        throw std::logic_error("CPU is already occupied");
    }
    if (clock_.now() < nextTickTime_) {
        throw std::logic_error("Clock must reach the end of the previous CPU tick");
    }
    process.setRunning(clock_.now());
    context_ = process.restoreContext();
    currentProcess_ = &process;
    nextTickTime_ = clock_.now();
}

CPU::TickResult CPU::tick() {
    if (isIdle()) {
        return TickResult::Idle;
    }
    if (!currentProcess_->isRunning()) {
        throw std::logic_error("The loaded process is not running");
    }
    if (clock_.now() != nextTickTime_) {
        throw std::logic_error("CPU tick requires the next consecutive clock time");
    }
    if (clock_.now() == std::numeric_limits<int>::max()) {
        throw std::overflow_error("No clock time remains for a CPU tick");
    }

    currentProcess_->updateRuntime(1);
    nextTickTime_ = clock_.now() + 1;
    // tick 只模拟运行时长，不把一个 tick 假定为一条机器指令。
    if (currentProcess_->isFinished()) {
        return TickResult::Finished;
    }
    if (currentProcess_->getTimeSlice() > 0 && currentProcess_->getRemainingTimeSlice() == 0) {
        return TickResult::TimeSliceExpired;
    }
    return TickResult::Running;
}

PCB* CPU::release() {
    if (isIdle()) {
        return nullptr;
    }
    if (clock_.now() < nextTickTime_) {
        throw std::logic_error("Advance the clock before releasing the CPU after a tick");
    }
    if (currentProcess_->isRunning()) {
        if (currentProcess_->isFinished()) {
            throw std::logic_error("Terminate the finished process before releasing the CPU");
        }
        currentProcess_->setReady();
    }
    currentProcess_->saveContext(context_);
    PCB* process = currentProcess_;
    currentProcess_ = nullptr;
    context_ = {};
    return process;
}

void CPU::setContext(const PCB::CPUContext& context) {
    if (isIdle() || !currentProcess_->isRunning()) {
        throw std::logic_error("Only a running process can change the CPU context");
    }
    context_ = context;
}
