//
// Created by YuHao Shi on 2026/11/6.
//

#include "../include/ProcessControlBlock.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
template<typename T>
void addUnique(std::vector<T>& values, const T& value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

template<typename T>
bool removeValue(std::vector<T>& values, const T& value) {
    return std::erase(values, value) != 0;
}
}

ProcessControlBlock::ProcessControlBlock(int pid, std::string name, int burstTime,
                                       int priority, int ppid, int arrivalTime)
    : pid_(pid), ppid_(ppid), name_(std::move(name)), priority_(priority),
      arrivalTime_(arrivalTime), burstTime_(burstTime), remainingTime_(burstTime),
      lastTime_(arrivalTime) {
    if (pid < 0 || ppid < -1 || pid == ppid || name_.empty() ||
        burstTime < 0 || arrivalTime < 0) {
        throw std::invalid_argument("Invalid process identity or timing parameters");
    }
}

void ProcessControlBlock::setReady() {
    if (state_ != ProcessState::New && state_ != ProcessState::Running && !isReady()) {
        throw std::logic_error("Only new, running or ready processes can become ready");
    }
    state_ = ProcessState::Ready;
}

void ProcessControlBlock::validateCurrentTime(int currentTime) const {
    if (currentTime < lastTime_) {
        throw std::invalid_argument("Current time cannot precede the process clock");
    }
}

void ProcessControlBlock::setRunning(int currentTime) {
    if (!isReady() || isFinished()) {
        throw std::logic_error("Only a ready process with remaining work can run");
    }
    validateCurrentTime(currentTime);
    if (!startTime_) {
        startTime_ = currentTime;
    }
    lastTime_ = currentTime;
    state_ = ProcessState::Running;
}

void ProcessControlBlock::setBlocked(const std::string& reason) {
    if (!isRunning()) {
        throw std::logic_error("Only a running process can block");
    }
    if (reason.empty()) {
        throw std::invalid_argument("A blocking reason is required");
    }
    blockedReason_ = reason;
    state_ = ProcessState::Blocked;
}

void ProcessControlBlock::wakeup() {
    if (isBlocked()) {
        state_ = ProcessState::Ready;
    } else if (isSuspended() && suspendedState_ == ProcessState::Blocked) {
        // 阻塞事件已完成，但进程仍保持挂起，等待显式激活。
        suspendedState_ = ProcessState::Ready;
    } else {
        throw std::logic_error("Only a blocked process can be awakened");
    }
    blockedReason_.clear();
    waitingResource_.reset();
}

void ProcessControlBlock::suspend() {
    if (!isReady() && !isBlocked()) {
        throw std::logic_error("Only ready or blocked processes can be suspended");
    }
    suspendedState_ = state_;
    state_ = ProcessState::Suspended;
}

void ProcessControlBlock::resume() {
    if (!isSuspended()) {
        throw std::logic_error("Only a suspended process can be resumed");
    }
    state_ = suspendedState_.value();
    suspendedState_.reset();
}

void ProcessControlBlock::terminate(int currentTime, int exitCode) {
    if (isTerminated()) {
        throw std::logic_error("Process is already terminated");
    }
    validateCurrentTime(currentTime);
    finishTime_ = currentTime;
    lastTime_ = currentTime;
    exitCode_ = exitCode;
    state_ = ProcessState::Terminated;
    suspendedState_.reset();
    blockedReason_.clear();
    waitingResource_.reset();
    remainingTimeSlice_ = 0;
    // 不清空资源/内存记录，进程管理器仍需依据这些记录执行回收。
}

std::optional<int> ProcessControlBlock::getTurnaroundTime() const {
    if (!finishTime_) {
        return std::nullopt;
    }
    return *finishTime_ - arrivalTime_;
}

void ProcessControlBlock::resetTimeSlice(int slice) {
    if (isTerminated()) {
        throw std::logic_error("Cannot assign a time slice to a terminated process");
    }
    if (slice <= 0) {
        throw std::invalid_argument("Time slice must be positive");
    }
    timeSlice_ = slice;
    remainingTimeSlice_ = slice;
}

void ProcessControlBlock::updateRuntime(int ticks) {
    if (!isRunning()) {
        throw std::logic_error("Only a running process can consume CPU time");
    }
    if (ticks <= 0 || ticks > remainingTime_) {
        throw std::invalid_argument("Runtime must be positive and within remaining work");
    }
    if (timeSlice_ > 0 && ticks > remainingTimeSlice_) {
        throw std::logic_error("Runtime exceeds the remaining time slice");
    }
    if (ticks > std::numeric_limits<int>::max() - lastTime_) {
        throw std::overflow_error("Process clock overflow");
    }
    cpuTime_ += ticks;
    remainingTime_ -= ticks;
    lastTime_ += ticks;
    if (timeSlice_ > 0) {
        remainingTimeSlice_ -= ticks;
    }
}

bool ProcessControlBlock::consumeTimeSlice() {
    if (timeSlice_ == 0) {
        throw std::logic_error("No time slice has been assigned");
    }
    updateRuntime(1);
    return remainingTimeSlice_ == 0;
}

void ProcessControlBlock::updateWaitTime(int ticks) {
    if (!isReady()) {
        throw std::logic_error("Only ready processes accumulate ready-queue wait time");
    }
    if (ticks < 0) {
        throw std::invalid_argument("Wait time cannot be negative");
    }
    if (ticks > std::numeric_limits<int>::max() - waitTime_ ||
        ticks > std::numeric_limits<int>::max() - lastTime_) {
        throw std::overflow_error("Wait time or process clock overflow");
    }
    waitTime_ += ticks;
    lastTime_ += ticks;
}

void ProcessControlBlock::setMemoryInfo(std::uint64_t base, std::size_t size) {
    if (size == 0 || size - 1 > std::numeric_limits<std::uint64_t>::max() - base) {
        throw std::invalid_argument("Invalid memory range");
    }
    memoryBase_ = base;
    memorySize_ = size;
}

void ProcessControlBlock::clearMemoryInfo() {
    memoryBase_ = 0;
    memorySize_ = 0;
    pageTable_.clear();
    isSwapped_ = false;
}

void ProcessControlBlock::setPageTable(std::vector<std::size_t> pageTable) {
    pageTable_ = std::move(pageTable);
}

void ProcessControlBlock::addResource(int resourceId) {
    if (resourceId < 0) {
        throw std::invalid_argument("Resource ID cannot be negative");
    }
    addUnique(heldResources_, resourceId);
}

bool ProcessControlBlock::releaseResource(int resourceId) {
    return removeValue(heldResources_, resourceId);
}

void ProcessControlBlock::setWaitingResource(int resourceId) {
    if (resourceId < 0) {
        throw std::invalid_argument("Resource ID cannot be negative");
    }
    waitingResource_ = resourceId;
}

void ProcessControlBlock::addChild(int pid) {
    if (pid < 0 || pid == pid_) {
        throw std::invalid_argument("Invalid child PID");
    }
    addUnique(children_, pid);
}

bool ProcessControlBlock::removeChild(int pid) {
    return removeValue(children_, pid);
}

void ProcessControlBlock::addOpenFile(const std::string& filename) {
    if (filename.empty()) {
        throw std::invalid_argument("Filename cannot be empty");
    }
    addUnique(openFiles_, filename);
}

bool ProcessControlBlock::removeOpenFile(const std::string& filename) {
    return removeValue(openFiles_, filename);
}

std::string_view ProcessControlBlock::stateToString(ProcessState state) {
    switch (state) {
        case ProcessState::New: return "New";
        case ProcessState::Ready: return "Ready";
        case ProcessState::Running: return "Running";
        case ProcessState::Blocked: return "Blocked";
        case ProcessState::Suspended: return "Suspended";
        case ProcessState::Terminated: return "Terminated";
    }
    return "Unknown";
}

std::string ProcessControlBlock::toString() const {
    std::ostringstream output;
    output << "PCB{pid=" << pid_ << ", ppid=" << ppid_ << ", name=" << name_
           << ", state=" << stateToString(state_) << ", priority=" << priority_
           << ", arrival=" << arrivalTime_ << ", burst=" << burstTime_
           << ", remaining=" << remainingTime_ << ", cpu=" << cpuTime_
           << ", wait=" << waitTime_ << ", slice=" << remainingTimeSlice_
           << '/' << timeSlice_ << ", memoryBase=" << memoryBase_
           << ", memorySize=" << memorySize_ << ", swapped=" << std::boolalpha << isSwapped_;
    if (startTime_) output << ", start=" << *startTime_;
    if (finishTime_) output << ", finish=" << *finishTime_;
    if (exitCode_) output << ", exitCode=" << *exitCode_;
    if (!blockedReason_.empty()) output << ", blockedReason=" << blockedReason_;
    if (suspendedState_) output << ", suspendedFrom=" << stateToString(*suspendedState_);
    output << '}';
    return output.str();
}

void ProcessControlBlock::print(std::ostream& output) const {
    output << toString() << '\n';
}

void ProcessControlBlock::print() const {
    print(std::cout);
}
