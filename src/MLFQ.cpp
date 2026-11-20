//
// Created by YuHao Shi on 2026/11/13.
//

#include "../include/MLFQ.h"

#include <stdexcept>

MLFQ::MLFQ(CPU& cpu, int timePiece) : timePiece(timePiece), cpu_(cpu) {
    if (timePiece <= 0) {
        throw std::invalid_argument("MLFQ time piece must be positive");
    }
}

std::array<std::queue<PCB*>*, MLFQ::kLevels> MLFQ::queues() noexcept {
    return {&Ready_processes1, &Ready_processes2, &Ready_processes3, &Ready_processes4,
            &Ready_processes5};
}

void MLFQ::receivePCB(PCB& pcb, int level) {
    if (level < 1 || level > kLevels) {
        throw std::invalid_argument("MLFQ level must be in [1, 5]");
    }
    if (pcb.isTerminated()) {
        throw std::logic_error("Cannot enqueue a terminated process");
    }
    if (pcb.isRunning()) {
        throw std::logic_error("Cannot enqueue the running process");
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
    queues()[level - 1]->push(&pcb);
}

bool MLFQ::ProcessShifter() {
    if (!cpu_.isIdle()) {
        return false;
    }
    if (timePiece <= 0) {
        throw std::logic_error("MLFQ time piece must be positive");
    }

    for (std::queue<PCB*>* queue : queues()) {
        if (queue->empty()) {
            continue;
        }

        PCB* process = queue->front();
        if (process == nullptr) {
            throw std::logic_error("MLFQ ready queue contains a null process");
        }
        // 队列里只应存在可运行的 Ready 进程；发现脏数据时保留原队列，便于定位问题。
        if (!process->isReady() || process->isFinished()) {
            throw std::logic_error("MLFQ ready queue contains a process that cannot run");
        }

        cpu_.dispatch(*process); // 派发失败（如进程尚未到达）时进程仍留在队首
        process->resetTimeSlice(timePiece);
        queue->pop();
        return true;
    }
    return false;
}
