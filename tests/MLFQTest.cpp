#include "../include/MLFQ.h"

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

struct Segment {
    int pid;
    int level;
    int start;
    int end;
};

void checkSegments(const std::vector<Segment>& segments,
                   const std::vector<std::vector<int>>& expected) {
    check(segments.size() == expected.size(), "Execution segment count");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        check(segments[i].pid == expected[i][0] && segments[i].level == expected[i][1] &&
                  segments[i].start == expected[i][2] && segments[i].end == expected[i][3],
              "Execution segment mismatch at index " + std::to_string(i));
    }
}

PCB* findByPid(const std::vector<PCB*>& processes, int pid) {
    for (PCB* process : processes) {
        if (process->getPid() == pid) {
            return process;
        }
    }
    return nullptr;
}

// 按调用方职责驱动 CPU：派发 -> tick -> 推进时钟 -> 处理完成/时间片耗尽。
std::vector<Segment> runToCompletion(CLOCK& clock, CPU& cpu, MLFQ& mlfq,
                                     const std::vector<PCB*>& processes) {
    std::vector<Segment> segments;
    PCB* running = nullptr;
    while (true) {
        if (running == nullptr) {
            if (!mlfq.ProcessShifter()) {
                break;
            }
            running = findByPid(processes, cpu.getCurrentProcess()->getPid());
        }
        const int pid = running->getPid();
        const int level = mlfq.getLevelOf(pid);
        const int now = clock.now();
        const CPU::TickResult result = cpu.tick();
        if (!segments.empty() && segments.back().pid == pid && segments.back().level == level &&
            segments.back().end == now) {
            segments.back().end = now + 1;
        } else {
            segments.push_back({pid, level, now, now + 1});
        }
        clock.advance();

        if (result == CPU::TickResult::Finished) {
            running->terminate(clock.now());
            cpu.release();
            mlfq.ProcessFinished(*running);
            running = nullptr;
        } else if (result == CPU::TickResult::TimeSliceExpired) {
            cpu.release();
            mlfq.TimeSliceExpired(*running);
            running = nullptr;
        }
    }
    return segments;
}

void testReceiveAndValidation() {
    CLOCK clock;
    CPU cpu(0, clock);
    MLFQ mlfq(cpu, 2);
    check(mlfq.getReadyCount() == 0 && mlfq.getLevelOf(1) == -1, "New MLFQ is empty");

    PCB first(1, "P1", 3);
    mlfq.receivePCB(first);
    check(first.isReady() && mlfq.getLevelOf(1) == 1 && mlfq.getQueueSize(1) == 1,
          "New process enters level 1 and becomes ready");
    expectThrow<std::logic_error>([&] { mlfq.receivePCB(first); });
    expectThrow<std::invalid_argument>([&] { mlfq.receivePCB(first, 0); });
    expectThrow<std::invalid_argument>([&] { mlfq.receivePCB(first, 6); });

    PCB zero(2, "zero", 0);
    expectThrow<std::logic_error>([&] { mlfq.receivePCB(zero); });

    PCB terminated(3, "done", 1);
    terminated.setReady();
    terminated.terminate(0);
    expectThrow<std::logic_error>([&] { mlfq.receivePCB(terminated); });

    PCB running(4, "run", 3);
    running.setReady();
    cpu.dispatch(running);
    expectThrow<std::logic_error>([&] { mlfq.receivePCB(running); });
    cpu.release();

    expectThrow<std::invalid_argument>([&] { (void)mlfq.getQueueSize(0); });
    expectThrow<std::invalid_argument>([&] { (void)mlfq.getQueueSize(6); });
}

void testProcessShifterOrder() {
    CLOCK clock;
    CPU cpu(0, clock);
    MLFQ mlfq(cpu, 3);
    PCB first(1, "P1", 5);
    PCB second(2, "P2", 5);
    PCB third(3, "P3", 5);
    mlfq.receivePCB(first, 1);
    mlfq.receivePCB(third, 1);
    mlfq.receivePCB(second, 3);

    check(mlfq.ProcessShifter(), "Dispatch from the highest non-empty queue");
    check(cpu.getCurrentProcess()->getPid() == 1 && first.isRunning(),
          "Level 1 is dispatched first");
    check(first.getTimeSlice() == 3, "Time piece is applied on dispatch");
    check(mlfq.getLevelOf(1) == 1 && mlfq.getQueueSize(1) == 1,
          "Dispatched process leaves the ready queue");
    check(!mlfq.ProcessShifter(), "Busy CPU rejects another dispatch");

    cpu.release();
    check(mlfq.ProcessShifter() && cpu.getCurrentProcess()->getPid() == 3, "Same-level FCFS order");
    cpu.release();
    check(mlfq.ProcessShifter() && cpu.getCurrentProcess()->getPid() == 2,
          "Level 1 is served before level 3");
    cpu.release();
    check(!mlfq.ProcessShifter() && mlfq.getReadyCount() == 0, "No queued process remains");

    mlfq.receivePCB(first, 1);
    check(mlfq.getLevelOf(1) == 1 && mlfq.getQueueSize(1) == 1,
          "A released process can be queued again");
}

void testDispatchFailureKeepsQueue() {
    CLOCK clock;
    CPU cpu(0, clock);
    MLFQ mlfq(cpu, 2);
    PCB future(1, "future", 2, 0, -1, 3);
    mlfq.receivePCB(future, 2);

    expectThrow<std::invalid_argument>([&] { (void)mlfq.ProcessShifter(); });
    check(cpu.isIdle() && mlfq.getQueueSize(2) == 1 && future.isReady(),
          "Failed dispatch keeps the process at the queue head");

    clock.advance(3);
    check(mlfq.ProcessShifter() && cpu.getCurrentProcess() == &future,
          "Dispatch succeeds once the process has arrived");
    cpu.release();
    mlfq.Preempted(future);
}

void testTimeSliceExpiredDemotes() {
    CLOCK clock;
    CPU cpu(0, clock);
    MLFQ mlfq(cpu, 2);
    PCB process(1, "long", 20);
    mlfq.receivePCB(process);

    check(mlfq.ProcessShifter() && mlfq.getLevelOf(1) == 1, "Start at level 1");
    check(cpu.tick() == CPU::TickResult::Running, "First tick consumes one time piece slot");
    clock.advance();
    check(cpu.tick() == CPU::TickResult::TimeSliceExpired, "Second tick exhausts the time piece");
    clock.advance();
    cpu.release();
    mlfq.TimeSliceExpired(process);
    check(mlfq.getLevelOf(1) == 2 && mlfq.getQueueSize(1) == 0 && mlfq.getQueueSize(2) == 1,
          "Time slice expiration demotes one level");

    check(mlfq.ProcessShifter() && process.getTimeSlice() == 2, "Demoted process gets a new slice");
    check(cpu.tick() == CPU::TickResult::Running, "Level 2 starts a fresh slice");
    clock.advance();
    check(cpu.tick() == CPU::TickResult::TimeSliceExpired, "Level 2 slice also expires");
    clock.advance();
    cpu.release();
    mlfq.TimeSliceExpired(process);
    check(mlfq.getLevelOf(1) == 3 && mlfq.getQueueSize(3) == 1,
          "Second expiration demotes to level 3");

    expectThrow<std::logic_error>([&] { mlfq.TimeSliceExpired(process); });
    expectThrow<std::logic_error>([&] { mlfq.Preempted(process); });

    CLOCK bottomClock;
    CPU bottomCpu(0, bottomClock);
    MLFQ bottom(bottomCpu, 2);
    PCB lowest(2, "lowest", 20);
    bottom.receivePCB(lowest, MLFQ::kLevels);
    check(bottom.ProcessShifter() && bottom.getLevelOf(2) == MLFQ::kLevels,
          "Lowest queue dispatch");
    check(bottomCpu.tick() == CPU::TickResult::Running, "Lowest queue runs");
    bottomClock.advance();
    check(bottomCpu.tick() == CPU::TickResult::TimeSliceExpired, "Lowest queue slice expires");
    bottomClock.advance();
    bottomCpu.release();
    bottom.TimeSliceExpired(lowest);
    check(bottom.getLevelOf(2) == MLFQ::kLevels && bottom.getQueueSize(MLFQ::kLevels) == 1,
          "Lowest level does not demote further");
}

void testPreemption() {
    CLOCK clock;
    CPU cpu(0, clock);
    MLFQ mlfq(cpu, 2);
    PCB low(1, "low", 10);
    PCB high(2, "high", 4);
    mlfq.receivePCB(low, 3);
    check(mlfq.ProcessShifter() && mlfq.getLevelOf(1) == 3, "Dispatch a level-3 process");
    check(!mlfq.ShouldPreempt(low), "No higher queue is waiting");

    mlfq.receivePCB(high, 2);
    check(mlfq.ShouldPreempt(low), "Higher-level arrival triggers preemption");
    cpu.release();
    mlfq.Preempted(low);
    check(mlfq.getLevelOf(1) == 3 && mlfq.getQueueSize(3) == 1, "Preemption keeps the level");

    check(mlfq.ProcessShifter() && cpu.getCurrentProcess()->getPid() == 2,
          "Higher queue is dispatched");
    check(!mlfq.ShouldPreempt(high), "Level 2 outranks nothing");
    PCB peer(3, "peer", 4);
    mlfq.receivePCB(peer, 2);
    check(!mlfq.ShouldPreempt(high), "Same-level arrival does not preempt");

    cpu.release();
    mlfq.Preempted(high);
}

void testBoostQueues() {
    CLOCK clock;
    CPU cpu(0, clock);
    MLFQ mlfq(cpu, 2);
    PCB first(1, "P1", 4);
    PCB second(2, "P2", 4);
    PCB third(3, "P3", 4);
    mlfq.receivePCB(first, 2);
    mlfq.receivePCB(second, 4);
    mlfq.receivePCB(third, MLFQ::kLevels);

    mlfq.BoostQueues();

    check(mlfq.getQueueSize(1) == 3 && mlfq.getReadyCount() == 3,
          "All waiting processes return to level 1");
    for (int level = 2; level <= MLFQ::kLevels; ++level) {
        check(mlfq.getQueueSize(level) == 0, "Lower queues are emptied by the boost");
    }
    check(mlfq.getLevelOf(1) == 1 && mlfq.getLevelOf(2) == 1 && mlfq.getLevelOf(3) == 1,
          "Boosted processes report level 1");

    const std::vector<int> expected = {1, 2, 3};
    for (int pid : expected) {
        check(mlfq.ProcessShifter() && cpu.getCurrentProcess()->getPid() == pid,
              "Boost keeps the order of the lower queues");
        cpu.release();
        mlfq.Preempted(*findByPid({&first, &second, &third}, pid));
    }
}

void testFullScenario() {
    CLOCK clock;
    CPU cpu(0, clock);
    MLFQ mlfq(cpu, 1);
    PCB first(1, "P1", 1);
    PCB second(2, "P2", 2);
    PCB third(3, "P3", 4);
    mlfq.receivePCB(first);
    mlfq.receivePCB(second);
    mlfq.receivePCB(third);

    const std::vector<Segment> segments =
        runToCompletion(clock, cpu, mlfq, {&first, &second, &third});

    checkSegments(segments, {{1, 1, 0, 1},
                             {2, 1, 1, 2},
                             {3, 1, 2, 3},
                             {2, 2, 3, 4},
                             {3, 2, 4, 5},
                             {3, 3, 5, 6},
                             {3, 4, 6, 7}});
    check(first.getFinishTime() == 1 && second.getFinishTime() == 4 && third.getFinishTime() == 7,
          "Short jobs finish first, long job sinks through the queues");
    check(mlfq.getReadyCount() == 0 && mlfq.getLevelOf(1) == -1 && mlfq.getLevelOf(3) == -1,
          "Queues and level records are cleared after completion");
}
} // namespace

int main() {
    try {
        testReceiveAndValidation();
        testProcessShifterOrder();
        testDispatchFailureKeepsQueue();
        testTimeSliceExpiredDemotes();
        testPreemption();
        testBoostQueues();
        testFullScenario();
        std::cout << "All MLFQ scheduler tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "MLFQ scheduler test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
