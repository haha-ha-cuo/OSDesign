#include "../include/CPU.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Result = CPU::TickResult;

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

void testInitialStateAndDispatch() {
    CLOCK clock;
    CPU cpu(0, clock);
    const CPU& readOnlyCpu = cpu;
    check(readOnlyCpu.getId() == 0 && readOnlyCpu.getStatus() == CPU::Status::FREE &&
              readOnlyCpu.isIdle() && !readOnlyCpu.getCurrentProcess(),
          "Initial CPU state");
    check(cpu.tick() == Result::Idle && cpu.release() == nullptr && clock.now() == 0,
          "Idle CPU does not advance the clock");
    expectThrow<std::invalid_argument>([&] { CPU invalid(-1, clock); });
    expectThrow<std::logic_error>([&] { cpu.setContext({}); });

    PCB pcb(1, "worker", 3);
    expectThrow<std::logic_error>([&] { cpu.dispatch(pcb); });
    check(cpu.isIdle() && pcb.getState() == PCB::ProcessState::New,
          "Rejected dispatch leaves CPU and PCB unchanged");
    pcb.setReady();
    cpu.dispatch(pcb);
    check(cpu.getStatus() == CPU::Status::OCCUPIED && cpu.getCurrentProcess() == &pcb &&
              pcb.isRunning() && pcb.getStartTime() == 0,
          "Dispatch a ready process");
    PCB another(2, "another", 1);
    another.setReady();
    expectThrow<std::logic_error>([&] { cpu.dispatch(another); });
    check(another.isReady(), "Busy CPU must not change the next process");
    check(cpu.release() == &pcb && pcb.isReady(), "Preempt before consuming any time");

    PCB zero(3, "zero", 0);
    zero.setReady();
    expectThrow<std::logic_error>([&] { cpu.dispatch(zero); });
    PCB future(4, "future", 1, 0, -1, 5);
    future.setReady();
    expectThrow<std::invalid_argument>([&] { cpu.dispatch(future); });
    check(cpu.isIdle(), "Invalid work or arrival time leaves CPU idle");
}

void testTimeSliceAndClockOrdering() {
    CLOCK clock;
    CPU cpu(0, clock);
    PCB pcb(1, "round-robin", 3);
    pcb.setReady();
    pcb.resetTimeSlice(2);
    cpu.dispatch(pcb);
    check(cpu.tick() == Result::Running && clock.now() == 0, "CPU does not advance global time");
    expectThrow<std::logic_error>([&] { cpu.tick(); });
    expectThrow<std::logic_error>([&] { cpu.release(); });
    check(pcb.getCpuTime() == 1, "Repeated tick must not count CPU time twice");
    clock.advance();
    check(cpu.tick() == Result::TimeSliceExpired, "Report exhausted time slice");
    clock.advance();
    expectThrow<std::logic_error>([&] { cpu.tick(); });
    check(pcb.getCpuTime() == 2 && pcb.isRunning(), "Scheduler handles time slice expiration");
    check(cpu.release() == &pcb && pcb.isReady(), "Preempt on time slice expiration");
    pcb.resetTimeSlice(1);
    cpu.dispatch(pcb);
    check(cpu.tick() == Result::Finished, "Completion takes precedence over time slice expiration");
    clock.advance();
    expectThrow<std::logic_error>([&] { cpu.release(); });
    pcb.terminate(clock.now());
    check(cpu.release() == &pcb && cpu.isIdle() && pcb.isTerminated() && pcb.getFinishTime() == 3,
          "Explicit termination and release");
}

void testContextSwitchAndBlocking() {
    CLOCK clock;
    CPU cpu(0, clock);
    PCB first(1, "first", 4);
    PCB second(2, "second", 2);
    first.setReady();
    second.setReady();
    PCB::CPUContext firstContext;
    firstContext.programCounter = 100;
    firstContext.registers[0] = 42;
    firstContext.stackPointer = 1024;
    first.saveContext(firstContext);
    cpu.dispatch(first);
    check(cpu.getContext() == firstContext, "Restore context on dispatch");
    firstContext.programCounter = 104;
    firstContext.registers[15] = 7;
    cpu.setContext(firstContext);
    check(cpu.tick() == Result::Running, "Execute without a time slice");
    second.updateWaitTime(1);
    clock.advance();
    cpu.release();
    check(first.restoreContext() == firstContext && cpu.getContext() == PCB::CPUContext{},
          "Save context on release and clear CPU registers");

    cpu.dispatch(second);
    check(cpu.getContext() == PCB::CPUContext{}, "Do not leak previous process context");
    cpu.tick();
    first.updateWaitTime(1);
    clock.advance();
    second.setBlocked("I/O");
    cpu.release();
    check(second.isBlocked() && second.getBlockedReason() == "I/O", "Preserve blocked state");

    cpu.dispatch(first);
    check(cpu.getContext() == firstContext, "Restore the preempted process context");
    first.terminate(clock.now(), -1);
    cpu.release();
    check(first.isTerminated() && first.restoreContext() == firstContext,
          "Forced termination still saves context");
}

void testClockBoundsAndExternalState() {
    CLOCK clock;
    CPU cpu(0, clock);
    PCB pcb(1, "worker", 5);
    pcb.setReady();
    cpu.dispatch(pcb);
    clock.advance(2);
    expectThrow<std::logic_error>([&] { cpu.tick(); });
    check(pcb.getCpuTime() == 0, "Do not silently skip CPU ticks");
    cpu.release();
    cpu.dispatch(pcb);
    pcb.setBlocked("I/O");
    expectThrow<std::logic_error>([&] { cpu.tick(); });
    cpu.release();
    check(pcb.isBlocked(), "Reject executing a blocked process");

    CLOCK limit(std::numeric_limits<int>::max());
    CPU limitCpu(1, limit);
    PCB limitProcess(2, "limit", 1, 0, -1, limit.now());
    limitProcess.setReady();
    limitCpu.dispatch(limitProcess);
    expectThrow<std::overflow_error>([&] { limitCpu.tick(); });
    check(limitProcess.getCpuTime() == 0 && limitProcess.getRemainingTime() == 1,
          "Clock overflow must not partially execute a process");
    limitCpu.release();
}
} // namespace

int main() {
    try {
        testInitialStateAndDispatch();
        testTimeSliceAndClockOrdering();
        testContextSwitchAndBlocking();
        testClockBoundsAndExternalState();
        std::cout << "All CPU tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "CPU test failed: " << error.what() << '\n';
        return 1;
    }
}
