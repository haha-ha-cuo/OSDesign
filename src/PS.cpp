//
// Created by YuHao Shi on 2026/11/13.
//

#include "../include/PS.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

PS::Options::Options() : preemptive(true), agingInterval(0), lowestPriority(0) {}

PS::PS(CLOCK& clock, CPU& cpu, Options options) : clock_(clock), cpu_(cpu), options_(options) {
    if (options_.agingInterval < 0) {
        throw std::invalid_argument("Aging interval cannot be negative");
    }
    if (options_.agingInterval > 0) {
        if (clock.now() > std::numeric_limits<int>::max() - options_.agingInterval) {
            throw std::overflow_error("No clock time remains for the aging interval");
        }
        nextAgingTick_ = clock.now() + options_.agingInterval;
    }
}

PCB& PS::addProcess(int pid, std::string name, int burstTime, int priority, int arrivalTime) {
    if (burstTime <= 0) {
        throw std::invalid_argument("Scheduler requires a positive burst time");
    }
    const bool duplicated = std::any_of(
        processes_.begin(), processes_.end(),
        [pid](const std::unique_ptr<PCB>& process) { return process->getPid() == pid; });
    if (duplicated) {
        throw std::invalid_argument("PID is already registered in this scheduler");
    }
    auto process =
        std::make_unique<PCB>(pid, std::move(name), burstTime, priority, -1, arrivalTime);
    PCB& reference = *process;
    processes_.push_back(std::move(process));
    return reference;
}

bool PS::isFinished() const {
    return std::all_of(processes_.begin(), processes_.end(),
                       [](const std::unique_ptr<PCB>& process) { return process->isTerminated(); });
}

bool PS::higherPriority(const PCB& candidate, const PCB& current) {
    if (candidate.getPriority() != current.getPriority()) {
        return candidate.getPriority() < current.getPriority();
    }
    if (candidate.getArrivalTime() != current.getArrivalTime()) {
        return candidate.getArrivalTime() < current.getArrivalTime();
    }
    return candidate.getPid() < current.getPid();
}

void PS::admitArrivals() {
    for (const auto& process : processes_) {
        // 优先级调度没有分级队列，到达时只要把状态改为 Ready 即可参与下一次选择。
        if (process->getState() == PCB::ProcessState::New &&
            process->getArrivalTime() <= clock_.now()) {
            process->setReady();
        }
    }
}

PCB* PS::pickNext() {
    PCB* best = nullptr;
    for (const auto& process : processes_) {
        if (!process->isReady()) {
            continue;
        }
        if (best == nullptr || higherPriority(*process, *best)) {
            best = process.get();
        }
    }
    return best;
}

bool PS::shouldPreempt(const PCB& running) const {
    if (!options_.preemptive) {
        return false;
    }
    return std::any_of(processes_.begin(), processes_.end(), [&running](const auto& process) {
        return process->isReady() && higherPriority(*process, running);
    });
}

void PS::recordExecution(const PCB& running, int now) {
    if (!timeline_.empty() && timeline_.back().pid == running.getPid() &&
        timeline_.back().endTime == now) {
        timeline_.back().endTime = now + 1;
        return;
    }
    timeline_.push_back({running.getPid(), running.getName(), now, now + 1});
}

void PS::onTickEnd() {
    if (options_.agingInterval <= 0 || clock_.now() < nextAgingTick_) {
        return;
    }
    // 老化只作用于等待中的就绪进程：新到达的进程没有积累等待时间，不会同步提升。
    for (const auto& process : processes_) {
        if (process->isReady()) {
            process->setPriority(std::max(options_.lowestPriority, process->getPriority() - 1));
        }
    }
    if (clock_.now() > std::numeric_limits<int>::max() - options_.agingInterval) {
        nextAgingTick_ = std::numeric_limits<int>::max();
        return;
    }
    nextAgingTick_ = clock_.now() + options_.agingInterval;
}

void PS::step() {
    if (isFinished()) {
        throw std::logic_error("The scheduler has no remaining process to run");
    }
    if (running_ != nullptr && cpu_.getCurrentProcess() != running_) {
        throw std::logic_error("The CPU was modified outside the scheduler");
    }
    if (running_ != nullptr && !running_->isRunning()) {
        throw std::logic_error("The running process left Running state outside the scheduler");
    }

    const int now = clock_.now();
    // 1. 处理当前时刻到达的作业，使其可以先于本轮调度参与优先级比较。
    admitArrivals();

    // 2. 抢占式下，更高优先级的进程到达后立即抢占当前进程。
    if (running_ != nullptr && shouldPreempt(*running_)) {
        PCB* preempted = cpu_.release();
        running_ = nullptr;
        // 被抢占的进程仍是 Ready，下一轮 pickNext() 会重新参与比较。
        (void)preempted;
    }

    // 3. 选择并装载下一个进程。
    if (running_ == nullptr) {
        running_ = pickNext();
        if (running_ != nullptr) {
            cpu_.dispatch(*running_);
        }
    }

    // 4. 执行一个 tick。
    CPU::TickResult result = CPU::TickResult::Idle;
    if (running_ != nullptr) {
        result = cpu_.tick();
        recordExecution(*running_, now);
        ++busyTicks_;
    }

    // 5. 其余就绪进程累计一个 tick 的等待时间。
    for (const auto& process : processes_) {
        if (process->isReady()) {
            process->updateWaitTime(1);
        }
    }

    // 6. 每个系统 tick 只推进一次全局时钟。
    ++totalTicks_;
    clock_.advance();

    // 7. 运行完成优先处理；优先级调度不分配时间片，正常不会出现 TimeSliceExpired。
    if (result == CPU::TickResult::Finished) {
        PCB* finished = running_;
        running_ = nullptr;
        finished->terminate(clock_.now());
        cpu_.release();
    } else if (result == CPU::TickResult::TimeSliceExpired) {
        // 若外部为进程设置了时间片，仍按就绪进程重新排队，不额外降级。
        cpu_.release();
        running_ = nullptr;
    }

    onTickEnd();
}

void PS::run(int maxTicks) {
    if (maxTicks <= 0) {
        throw std::invalid_argument("Tick limit must be positive");
    }
    int ticks = 0;
    while (!isFinished()) {
        if (ticks >= maxTicks) {
            throw std::overflow_error("Scheduling did not finish within the tick limit");
        }
        step();
        ++ticks;
    }
}

PS::Metrics PS::getMetrics() const {
    Metrics metrics;
    metrics.totalTicks = totalTicks_;
    metrics.busyTicks = busyTicks_;
    double turnaround = 0.0;
    double waiting = 0.0;
    double response = 0.0;
    for (const auto& process : processes_) {
        if (!process->isTerminated()) {
            continue;
        }
        ++metrics.finishedProcesses;
        turnaround += static_cast<double>(*process->getTurnaroundTime());
        waiting += static_cast<double>(process->getWaitTime());
        response += static_cast<double>(*process->getStartTime() - process->getArrivalTime());
    }
    if (metrics.finishedProcesses > 0) {
        const double count = static_cast<double>(metrics.finishedProcesses);
        metrics.averageTurnaroundTime = turnaround / count;
        metrics.averageWaitingTime = waiting / count;
        metrics.averageResponseTime = response / count;
    }
    if (totalTicks_ > 0) {
        metrics.cpuUtilization =
            100.0 * static_cast<double>(busyTicks_) / static_cast<double>(totalTicks_);
        metrics.throughput =
            static_cast<double>(metrics.finishedProcesses) / static_cast<double>(totalTicks_);
    }
    return metrics;
}
