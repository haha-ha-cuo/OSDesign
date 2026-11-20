//
// Created by YuHao Shi on 2026/11/13.
//

#include "../include/MLFQ.h"

#include <algorithm>
#include <stdexcept>

namespace {
void validateLevel(int level) {
    if (level < 1 || level > MLFQ::kLevels) {
        throw std::invalid_argument("MLFQ level must be in [1, 5]");
    }
}
} // namespace

MLFQ::MLFQ(CPU& cpu, int timePiece) : timePiece(timePiece), cpu_(cpu) {
    if (timePiece <= 0) {
        throw std::invalid_argument("MLFQ time piece must be positive");
    }
}

std::array<std::queue<PCB*>*, MLFQ::kLevels> MLFQ::queues() noexcept {
    return {&Ready_processes1, &Ready_processes2, &Ready_processes3, &Ready_processes4,
            &Ready_processes5};
}

std::array<const std::queue<PCB*>*, MLFQ::kLevels> MLFQ::queues() const noexcept {
    return {&Ready_processes1, &Ready_processes2, &Ready_processes3, &Ready_processes4,
            &Ready_processes5};
}

void MLFQ::enqueue(PCB& pcb, int level) {
    levelByPid_[pcb.getPid()] = level;
    queues()[level - 1]->push(&pcb);
}

void MLFQ::clearDispatch(int pid) {
    if (dispatchedPid_ == pid) {
        dispatchedPid_ = -1;
        dispatchedLevel_ = 0;
    }
}

void MLFQ::receivePCB(PCB& pcb, int level) {
    validateLevel(level);
    if (pcb.isTerminated()) {
        throw std::logic_error("Cannot enqueue a terminated process");
    }
    if (pcb.isRunning()) {
        throw std::logic_error("Cannot enqueue the running process");
    }
    if (levelByPid_.contains(pcb.getPid())) {
        throw std::logic_error("Process is already in a ready queue");
    }
    if (pcb.getState() == PCB::ProcessState::New) {
        pcb.setReady();
    }
    if (!pcb.isReady()) {
        throw std::logic_error("Only ready processes can enter the ready queue");
    }
    if (pcb.isFinished()) {
        throw std::logic_error("Cannot enqueue a process with no remaining work");
    }
    // 进程被抢占/降级后重新入队时，清除旧的派发记录，getLevelOf() 改读队列映射。
    clearDispatch(pcb.getPid());
    enqueue(pcb, level);
}

bool MLFQ::ProcessShifter() {
    if (!cpu_.isIdle()) {
        return false;
    }
    if (timePiece <= 0) {
        throw std::logic_error("MLFQ time piece must be positive");
    }

    for (int level = 1; level <= kLevels; ++level) {
        std::queue<PCB*>& queue = *queues()[level - 1];
        if (queue.empty()) {
            continue;
        }

        PCB* process = queue.front();
        if (process == nullptr) {
            throw std::logic_error("MLFQ ready queue contains a null process");
        }
        // 队列里只应存在可运行的 Ready 进程；发现脏数据时保留原队列，便于定位问题。
        if (!process->isReady() || process->isFinished()) {
            throw std::logic_error("MLFQ ready queue contains a process that cannot run");
        }

        cpu_.dispatch(*process); // 派发失败（如进程尚未到达）时进程仍留在队首
        process->resetTimeSlice(timePiece);
        queue.pop();
        levelByPid_.erase(process->getPid());
        dispatchedPid_ = process->getPid();
        dispatchedLevel_ = level;
        return true;
    }
    return false;
}

void MLFQ::TimeSliceExpired(PCB& pcb) {
    if (dispatchedPid_ != pcb.getPid()) {
        throw std::logic_error(
            "TimeSliceExpired() requires the process dispatched by ProcessShifter()");
    }
    if (!pcb.isReady()) {
        throw std::logic_error("Release the CPU before requeueing the process");
    }
    const int nextLevel = std::min(dispatchedLevel_ + 1, kLevels);
    clearDispatch(pcb.getPid());
    enqueue(pcb, nextLevel);
}

void MLFQ::Preempted(PCB& pcb) {
    if (dispatchedPid_ != pcb.getPid()) {
        throw std::logic_error("Preempted() requires the process dispatched by ProcessShifter()");
    }
    if (!pcb.isReady()) {
        throw std::logic_error("Release the CPU before requeueing the process");
    }
    const int level = dispatchedLevel_;
    clearDispatch(pcb.getPid());
    enqueue(pcb, level);
}

void MLFQ::ProcessFinished(PCB& pcb) {
    clearDispatch(pcb.getPid());
    levelByPid_.erase(pcb.getPid());
}

bool MLFQ::ShouldPreempt(const PCB& running) const {
    const int level = getLevelOf(running.getPid());
    if (level <= 1) {
        return false;
    }
    for (int candidate = 1; candidate < level; ++candidate) {
        if (!queues()[candidate - 1]->empty()) {
            return true;
        }
    }
    return false;
}

void MLFQ::BoostQueues() {
    for (int level = 2; level <= kLevels; ++level) {
        std::queue<PCB*>& source = *queues()[level - 1];
        while (!source.empty()) {
            PCB* process = source.front();
            source.pop();
            enqueue(*process, 1);
        }
    }
}

int MLFQ::getLevelOf(int pid) const {
    if (pid == dispatchedPid_) {
        return dispatchedLevel_;
    }
    const auto position = levelByPid_.find(pid);
    return position == levelByPid_.end() ? -1 : position->second;
}

int MLFQ::getQueueSize(int level) const {
    validateLevel(level);
    return static_cast<int>(queues()[level - 1]->size());
}

int MLFQ::getReadyCount() const {
    int total = 0;
    for (const std::queue<PCB*>* queue : queues()) {
        total += static_cast<int>(queue->size());
    }
    return total;
}
