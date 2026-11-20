#include "../include/PS.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Exception, typename Action>
void expectThrow(Action action) {
    try {
        action();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error("Expected exception was not thrown");
}

void checkTimeline(const std::vector<PS::TimelineEntry>& timeline,
                   const std::vector<std::vector<int>>& expected) {
    check(timeline.size() == expected.size(), "Timeline segment count");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        check(timeline[i].pid == expected[i][0] && timeline[i].startTime == expected[i][1] &&
                  timeline[i].endTime == expected[i][2],
              "Timeline segment mismatch at index " + std::to_string(i));
    }
}

void testPreemptiveOrderAndMetrics() {
    CLOCK clock;
    CPU cpu(0, clock);
    PS scheduler(clock, cpu);
    PCB& first = scheduler.addProcess(1, "P1", 5, 2, 0);
    PCB& second = scheduler.addProcess(2, "P2", 2, 1, 2);

    scheduler.run();

    check(scheduler.isFinished() && clock.now() == 7, "Preemptive run finishes at tick 7");
    checkTimeline(scheduler.getTimeline(), {{1, 0, 2}, {2, 2, 4}, {1, 4, 7}});
    check(first.getStartTime() == 0 && first.getFinishTime() == 7 && first.getWaitTime() == 2,
          "Preempted process resumes and records wait time");
    check(second.getStartTime() == 2 && second.getFinishTime() == 4 && second.getWaitTime() == 0,
          "Higher-priority arrival starts immediately");

    const PS::Metrics metrics = scheduler.getMetrics();
    check(std::abs(metrics.averageTurnaroundTime - 4.5) < 1e-9, "Average turnaround time");
    check(std::abs(metrics.averageWaitingTime - 1.0) < 1e-9, "Average waiting time");
    check(std::abs(metrics.averageResponseTime - 0.0) < 1e-9, "Average response time");
    check(std::abs(metrics.cpuUtilization - 100.0) < 1e-9, "CPU utilization");
    check(std::abs(metrics.throughput - 2.0 / 7.0) < 1e-9, "Throughput");
    check(metrics.finishedProcesses == 2 && metrics.busyTicks == 7 && metrics.totalTicks == 7,
          "Tick accounting");
}

void testNonPreemptive() {
    CLOCK clock;
    CPU cpu(0, clock);
    PS::Options options;
    options.preemptive = false;
    PS scheduler(clock, cpu, options);
    PCB& first = scheduler.addProcess(1, "P1", 5, 2, 0);
    PCB& second = scheduler.addProcess(2, "P2", 2, 1, 2);

    scheduler.run();

    checkTimeline(scheduler.getTimeline(), {{1, 0, 5}, {2, 5, 7}});
    check(first.getFinishTime() == 5 && second.getStartTime() == 5 && second.getFinishTime() == 7,
          "Non-preemptive scheduling ignores higher priority until completion");
}

void testTieBreaks() {
    CLOCK clock;
    CPU cpu(0, clock);
    PS scheduler(clock, cpu);
    // 同优先级、同到达时间：按 PID 升序。
    scheduler.addProcess(2, "P2", 1, 1, 0);
    scheduler.addProcess(1, "P1", 1, 1, 0);
    scheduler.addProcess(3, "P3", 1, 1, 0);
    scheduler.run();
    checkTimeline(scheduler.getTimeline(), {{1, 0, 1}, {2, 1, 2}, {3, 2, 3}});

    CLOCK arrivalClock;
    CPU arrivalCpu(0, arrivalClock);
    PS arrivalScheduler(arrivalClock, arrivalCpu);
    // 同优先级：先到达者先运行。
    arrivalScheduler.addProcess(2, "late", 1, 1, 1);
    arrivalScheduler.addProcess(1, "early", 1, 1, 0);
    arrivalScheduler.run();
    checkTimeline(arrivalScheduler.getTimeline(), {{1, 0, 1}, {2, 1, 2}});
}

void testAgingPreventsStarvation() {
    CLOCK clock;
    CPU cpu(0, clock);
    PS::Options options;
    options.agingInterval = 2;
    options.lowestPriority = 0;
    PS scheduler(clock, cpu, options);
    scheduler.addProcess(1, "low", 100, 1, 0);
    PCB& waiting = scheduler.addProcess(2, "waiter", 1, 9, 0);

    scheduler.run();

    // 每 2 个 tick 提升一级：9 -> 0 需要 9 次，tick 18 时开始运行。
    check(waiting.getStartTime() == 18 && waiting.getPriority() == 0,
          "Aging promotes the waiting process");
    check(waiting.getFinishTime() == 19, "Aged process finishes after its burst");

    CLOCK plainClock;
    CPU plainCpu(0, plainClock);
    PS plainScheduler(plainClock, plainCpu);
    plainScheduler.addProcess(1, "low", 100, 1, 0);
    PCB& starved = plainScheduler.addProcess(2, "waiter", 1, 9, 0);
    plainScheduler.run();
    check(starved.getStartTime() == 100, "Without aging the low-priority process waits");
}

void testValidationAndEmptySystem() {
    CLOCK clock;
    CPU cpu(0, clock);
    PS scheduler(clock, cpu);
    check(scheduler.isFinished(), "Empty scheduler is finished");
    check(scheduler.getTimeline().empty(), "Empty scheduler has no timeline");

    scheduler.addProcess(1, "P1", 2);
    expectThrow<std::invalid_argument>([&] { scheduler.addProcess(1, "dup", 1); });
    expectThrow<std::invalid_argument>([&] { scheduler.addProcess(2, "zero", 0); });
    expectThrow<std::invalid_argument>([&] { scheduler.run(0); });

    scheduler.run();
    check(scheduler.getTimeline().size() == 1, "Single process runs in one segment");

    PS::Options options;
    options.agingInterval = -1;
    expectThrow<std::invalid_argument>([&] { PS invalid(clock, cpu, options); });
}
} // namespace

int main() {
    try {
        testPreemptiveOrderAndMetrics();
        testNonPreemptive();
        testTieBreaks();
        testAgingPreventsStarvation();
        testValidationAndEmptySystem();
        std::cout << "All priority scheduler tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Priority scheduler test failed: " << error.what() << '\n';
        return 1;
    }
}
