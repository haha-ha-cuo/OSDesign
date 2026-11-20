#include "../include/MLFQ.h"

#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
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

void testReceiveAndDispatch() {
    CLOCK clock;
    CPU cpu(0, clock);
    MLFQ mlfq(cpu, 2);
    check(mlfq.timePiece == 2, "Store configured time piece");
    expectThrow<std::invalid_argument>([&] { MLFQ invalid(cpu, 0); });

    PCB process(1, "worker", 3);
    mlfq.receivePCB(process);
    check(process.isReady(), "New process becomes ready on receive");
    check(mlfq.Ready_processes1.size() == 1, "Received process enters the top queue");
    expectThrow<std::invalid_argument>([&] { mlfq.receivePCB(process, 0); });
    expectThrow<std::invalid_argument>([&] { mlfq.receivePCB(process, 6); });

    check(mlfq.ProcessShifter(), "Dispatch from the top queue");
    check(cpu.getCurrentProcess() == &process && process.isRunning(),
          "CPU loads the same PCB instance");
    check(mlfq.Ready_processes1.empty(), "Dispatched process leaves the ready queue");
    check(!mlfq.ProcessShifter(), "Busy CPU must not dispatch another process");
    expectThrow<std::logic_error>([&] { mlfq.receivePCB(process); });

    check(cpu.release() == &process && process.isReady(), "Preempt keeps the PCB reusable");
    mlfq.receivePCB(process);
    check(mlfq.ProcessShifter(), "Dispatch the preempted process again");
    check(cpu.release() == &process, "Release after re-dispatch");
}

void testHighestPriorityAndFcfs() {
    CLOCK clock;
    CPU cpu(0, clock);
    MLFQ mlfq(cpu, 4);

    PCB low(1, "low", 2);
    PCB highFirst(2, "high-first", 2);
    PCB highSecond(3, "high-second", 2);
    mlfq.receivePCB(low, 5);
    mlfq.receivePCB(highFirst, 2);
    mlfq.receivePCB(highSecond, 2);
    check(mlfq.Ready_processes5.size() == 1 && mlfq.Ready_processes2.size() == 2,
          "Processes enter their requested queues");

    check(mlfq.ProcessShifter(), "Pick the highest-priority non-empty queue");
    check(cpu.getCurrentProcess() == &highFirst, "FIFO order inside one queue");
    check(mlfq.Ready_processes2.size() == 1 && mlfq.Ready_processes5.size() == 1,
          "Only the dispatched process is removed");
    check(cpu.release() == &highFirst, "Release first high-priority process");

    check(mlfq.ProcessShifter(), "Continue with the same queue");
    check(cpu.getCurrentProcess() == &highSecond, "Second process of the queue runs next");
    check(mlfq.Ready_processes2.empty() && mlfq.Ready_processes5.size() == 1,
          "Lower queue still waits");
    check(cpu.release() == &highSecond, "Release second high-priority process");

    check(mlfq.ProcessShifter(), "Fall back to the lower queue when higher ones are empty");
    check(cpu.getCurrentProcess() == &low, "Lowest queue process finally runs");
    check(cpu.release() == &low, "Release the last process");
}

void testTimeSliceExpiryAndDemotion() {
    CLOCK clock;
    CPU cpu(0, clock);
    MLFQ mlfq(cpu, 2);
    PCB process(1, "batch", 5);

    mlfq.receivePCB(process);
    check(mlfq.ProcessShifter(), "Dispatch the first slice");
    check(cpu.tick() == CPU::TickResult::Running, "First tick of the slice");
    clock.advance();
    check(cpu.tick() == CPU::TickResult::TimeSliceExpired, "Second tick exhausts the slice");
    clock.advance();
    check(process.getCpuTime() == 2 && process.getRemainingTime() == 3,
          "Only executed ticks are counted");
    check(cpu.release() == &process && process.isReady(), "Release the expired process");

    mlfq.receivePCB(process, 2);
    check(mlfq.Ready_processes1.empty() && mlfq.Ready_processes2.size() == 1,
          "Demote to the next queue");
    check(mlfq.ProcessShifter(), "Dispatch from the lower queue");
    check(process.getRemainingTimeSlice() == 2, "Time slice is reset for the new queue");

    while (!process.isFinished()) {
        const auto result = cpu.tick();
        clock.advance();
        if (result == CPU::TickResult::Finished) {
            process.terminate(clock.now());
            cpu.release();
        } else if (result == CPU::TickResult::TimeSliceExpired) {
            cpu.release();
            mlfq.receivePCB(process, 3);
            check(mlfq.ProcessShifter(), "Re-dispatch after another expiry");
        }
    }
    check(process.isTerminated() && cpu.isIdle(), "Finished process leaves the CPU");
    check(process.getCpuTime() == 5 && process.getRemainingTime() == 0,
          "All work is accounted for");
    check(!mlfq.ProcessShifter(), "Empty scheduler does nothing");
}

void testRejectInvalidProcesses() {
    CLOCK clock;
    CPU cpu(0, clock);
    MLFQ mlfq(cpu, 4);

    PCB running(1, "running", 2);
    running.setReady();
    cpu.dispatch(running);
    expectThrow<std::logic_error>([&] { mlfq.receivePCB(running); });
    running.setBlocked("I/O");
    cpu.release();
    expectThrow<std::logic_error>([&] { mlfq.receivePCB(running); });

    PCB terminated(2, "terminated", 1);
    terminated.terminate(0);
    expectThrow<std::logic_error>([&] { mlfq.receivePCB(terminated); });

    check(mlfq.Ready_processes1.empty(), "Rejected processes are not queued");
}

void testDirtyQueueFailsLoudly() {
    CLOCK clock;
    CPU cpu(0, clock);
    MLFQ mlfq(cpu);

    mlfq.Ready_processes1.push(nullptr);
    expectThrow<std::logic_error>([&] { mlfq.ProcessShifter(); });
    check(mlfq.Ready_processes1.size() == 1, "Bad entry is kept for diagnosis");

    mlfq.Ready_processes1.pop();
    PCB blocked(1, "blocked", 2);
    blocked.setReady();
    mlfq.Ready_processes1.push(&blocked);
    blocked.suspend();
    expectThrow<std::logic_error>([&] { mlfq.ProcessShifter(); });
    check(mlfq.Ready_processes1.size() == 1, "Non-ready entry is not silently dropped");
}
} // namespace

int main() {
    try {
        testReceiveAndDispatch();
        testHighestPriorityAndFcfs();
        testTimeSliceExpiryAndDemotion();
        testRejectInvalidProcesses();
        testDirtyQueueFailsLoudly();
        std::cout << "All MLFQ tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "MLFQ test failed: " << error.what() << '\n';
        return 1;
    }
}
