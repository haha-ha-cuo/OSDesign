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

// ====================================================================
// 以下为本地改写版（已整体注释，仅作备份，不参与编译）
// 来源分支：backup/wip-local-mlfq-ps:src/MLFQ.cpp
// 与当前主线版 MLFQ 的 API 不兼容；需要恢复时，把下面内容复制到文件顶部替换。
// ====================================================================
//
// Created by YuHao Shi on 2026/11/13.
//

// #include "../include/MLFQ.h"

// #include <algorithm>
// #include <limits>
// #include <stdexcept>
// #include <utility>

// MLFQ::Options::Options()
//     : levels(5), baseSlice(1), sliceGrowth(2), preemptive(true), boostInterval(0) {}

// MLFQ::MLFQ(CLOCK& clock, CPU& cpu, Options options) : clock_(clock), cpu_(cpu), options_(options) {
//     if (options_.levels < 1) {
//         throw std::invalid_argument("At least one feedback queue is required");
//     }
//     if (options_.baseSlice < 1) {
//         throw std::invalid_argument("The base time slice must be positive");
//     }
//     if (options_.sliceGrowth < 1) {
//         throw std::invalid_argument("The time slice growth factor must be at least one");
//     }
//     if (options_.boostInterval < 0) {
//         throw std::invalid_argument("The boost interval cannot be negative");
//     }
// 提前校验最长时间片，避免运行到低级别队列时才溢出。
//     long long slice = options_.baseSlice;
//     for (int level = 0; level < options_.levels; ++level) {
//         if (slice > std::numeric_limits<int>::max()) {
//             throw std::invalid_argument("Time slice for the lowest queue exceeds the int range");
//         }
//         slice *= options_.sliceGrowth;
//     }
//     queues_.resize(static_cast<std::size_t>(options_.levels));
//     if (options_.boostInterval > 0) {
//         if (clock.now() > std::numeric_limits<int>::max() - options_.boostInterval) {
//             throw std::overflow_error("No clock time remains for the boost interval");
//         }
//         nextBoostTick_ = clock.now() + options_.boostInterval;
//     }
// }

// PCB& MLFQ::addProcess(int pid, std::string name, int burstTime, int priority, int arrivalTime) {
//     if (burstTime <= 0) {
//         throw std::invalid_argument("Scheduler requires a positive burst time");
//     }
//     const bool duplicated = std::any_of(
//         processes_.begin(), processes_.end(),
//         [pid](const std::unique_ptr<PCB>& process) { return process->getPid() == pid; });
//     if (duplicated) {
//         throw std::invalid_argument("PID is already registered in this scheduler");
//     }
//     auto process =
//         std::make_unique<PCB>(pid, std::move(name), burstTime, priority, -1, arrivalTime);
//     PCB& reference = *process;
//     processes_.push_back(std::move(process));
//     return reference;
// }

// bool MLFQ::isFinished() const {
//     return std::all_of(processes_.begin(), processes_.end(),
//                        [](const std::unique_ptr<PCB>& process) { return process->isTerminated(); });
// }

// int MLFQ::getTimeSlice(int level) const {
//     validateLevel(level);
//     long long slice = options_.baseSlice;
//     for (int current = 0; current < level; ++current) {
//         slice *= options_.sliceGrowth;
//     }
//     return static_cast<int>(slice);
// }

// int MLFQ::getQueueSize(int level) const {
//     validateLevel(level);
//     return static_cast<int>(queues_[static_cast<std::size_t>(level)].size());
// }

// int MLFQ::getLevelOf(int pid) const {
//     const auto position = levelByPid_.find(pid);
//     return position == levelByPid_.end() ? -1 : position->second;
// }

// void MLFQ::validateLevel(int level) const {
//     if (level < 0 || level >= options_.levels) {
//         throw std::invalid_argument("Queue level is out of range");
//     }
// }

// int MLFQ::levelOf(const PCB& process) const {
//     return getLevelOf(process.getPid());
// }

// void MLFQ::enqueue(PCB& process, int level) {
//     levelByPid_[process.getPid()] = level;
//     queues_[static_cast<std::size_t>(level)].push_back(&process);
// }

// void MLFQ::admitArrivals() {
//     for (const auto& process : processes_) {
//         if (process->getState() == PCB::ProcessState::New &&
//             process->getArrivalTime() <= clock_.now()) {
//             process->setReady();
//             enqueue(*process, 0);
//         }
//     }
// }

// PCB* MLFQ::pickNext() {
//     for (int level = 0; level < options_.levels; ++level) {
//         auto& queue = queues_[static_cast<std::size_t>(level)];
//         if (queue.empty()) {
//             continue;
//         }
//         PCB* next = queue.front();
//         queue.pop_front();
// 时间片按队列级别重新分配，被抢占的进程不继承上一轮的剩余量。
//         next->resetTimeSlice(getTimeSlice(level));
//         return next;
//     }
//     return nullptr;
// }

// bool MLFQ::shouldPreempt(const PCB& running) const {
//     if (!options_.preemptive) {
//         return false;
//     }
//     const int runningLevel = levelOf(running);
//     if (runningLevel < 0) {
//         return false;
//     }
// 只有更高级别队列非空才抢占；同级别仍按先来先服务。
//     for (int level = 0; level < runningLevel; ++level) {
//         if (!queues_[static_cast<std::size_t>(level)].empty()) {
//             return true;
//         }
//     }
//     return false;
// }

// void MLFQ::onTimeSliceExpired(PCB& process) {
//     const int current = levelOf(process);
//     const int nextLevel = std::min(current + 1, options_.levels - 1);
//     enqueue(process, nextLevel);
// }

// void MLFQ::onPreempted(PCB& process) {
// 抢占不降级：进程回到原队列尾部，等待更高级别队列清空。
//     enqueue(process, levelOf(process));
// }

// void MLFQ::onFinished(PCB& process) {
//     levelByPid_.erase(process.getPid());
// }

// void MLFQ::onTickEnd() {
//     if (options_.boostInterval <= 0 || clock_.now() < nextBoostTick_) {
//         return;
//     }
// 周期性提升：所有等待中的低级别进程重新回到最高级队列。
//     for (int level = 1; level < options_.levels; ++level) {
//         auto& queue = queues_[static_cast<std::size_t>(level)];
//         while (!queue.empty()) {
//             PCB* process = queue.front();
//             queue.pop_front();
//             levelByPid_[process->getPid()] = 0;
//             queues_[0].push_back(process);
//         }
//     }
//     if (clock_.now() > std::numeric_limits<int>::max() - options_.boostInterval) {
//         nextBoostTick_ = std::numeric_limits<int>::max();
//         return;
//     }
//     nextBoostTick_ = clock_.now() + options_.boostInterval;
// }

// void MLFQ::recordExecution(const PCB& running, int now) {
// 连续 tick 执行同一进程时合并片段；队列级别变化时另起一段，便于观察降级过程。
//     const int level = levelOf(running);
//     if (!timeline_.empty() && timeline_.back().pid == running.getPid() &&
//         timeline_.back().endTime == now && timeline_.back().queueLevel == level) {
//         timeline_.back().endTime = now + 1;
//         return;
//     }
//     timeline_.push_back({running.getPid(), running.getName(), now, now + 1, level});
// }

// void MLFQ::step() {
//     if (isFinished()) {
//         throw std::logic_error("The scheduler has no remaining process to run");
//     }
//     if (running_ != nullptr && cpu_.getCurrentProcess() != running_) {
//         throw std::logic_error("The CPU was modified outside the scheduler");
//     }
//     if (running_ != nullptr && !running_->isRunning()) {
//         throw std::logic_error("The running process left Running state outside the scheduler");
//     }

//     const int now = clock_.now();
// 1. 处理当前时刻到达的作业，使其可以先于本轮调度进入就绪队列。
//     admitArrivals();

// 2. 高级别队列有进程时抢占当前进程。
//     if (running_ != nullptr && shouldPreempt(*running_)) {
//         PCB* preempted = cpu_.release();
//         running_ = nullptr;
//         onPreempted(*preempted);
//     }

// 3. 选择并装载下一个进程。
//     if (running_ == nullptr) {
//         running_ = pickNext();
//         if (running_ != nullptr) {
//             cpu_.dispatch(*running_);
//         }
//     }

// 4. 执行一个 tick，并登记甘特图片段。
//     CPU::TickResult result = CPU::TickResult::Idle;
//     if (running_ != nullptr) {
//         result = cpu_.tick();
//         recordExecution(*running_, now);
//         ++busyTicks_;
//     }

// 5. 其余就绪进程累计一个 tick 的等待时间。
//     for (const auto& process : processes_) {
//         if (process->isReady()) {
//             process->updateWaitTime(1);
//         }
//     }

// 6. 每个系统 tick 只推进一次全局时钟。
//     ++totalTicks_;
//     clock_.advance();

// 7. 完成优先于时间片耗尽，二者都在时钟推进后处理。
//     if (result == CPU::TickResult::Finished) {
//         PCB* finished = running_;
//         running_ = nullptr;
//         finished->terminate(clock_.now());
//         cpu_.release();
//         onFinished(*finished);
//     } else if (result == CPU::TickResult::TimeSliceExpired) {
//         PCB* expired = cpu_.release();
//         running_ = nullptr;
//         onTimeSliceExpired(*expired);
//     }

//     onTickEnd();
// }

// void MLFQ::run(int maxTicks) {
//     if (maxTicks <= 0) {
//         throw std::invalid_argument("Tick limit must be positive");
//     }
//     int ticks = 0;
//     while (!isFinished()) {
//         if (ticks >= maxTicks) {
//             throw std::overflow_error("Scheduling did not finish within the tick limit");
//         }
//         step();
//         ++ticks;
//     }
// }

// MLFQ::Metrics MLFQ::getMetrics() const {
//     Metrics metrics;
//     metrics.totalTicks = totalTicks_;
//     metrics.busyTicks = busyTicks_;
//     double turnaround = 0.0;
//     double waiting = 0.0;
//     double response = 0.0;
//     for (const auto& process : processes_) {
//         if (!process->isTerminated()) {
//             continue;
//         }
//         ++metrics.finishedProcesses;
//         turnaround += static_cast<double>(*process->getTurnaroundTime());
//         waiting += static_cast<double>(process->getWaitTime());
//         response += static_cast<double>(*process->getStartTime() - process->getArrivalTime());
//     }
//     if (metrics.finishedProcesses > 0) {
//         const double count = static_cast<double>(metrics.finishedProcesses);
//         metrics.averageTurnaroundTime = turnaround / count;
//         metrics.averageWaitingTime = waiting / count;
//         metrics.averageResponseTime = response / count;
//     }
//     if (totalTicks_ > 0) {
//         metrics.cpuUtilization =
//             100.0 * static_cast<double>(busyTicks_) / static_cast<double>(totalTicks_);
//         metrics.throughput =
//             static_cast<double>(metrics.finishedProcesses) / static_cast<double>(totalTicks_);
//     }
//     return metrics;
// }
