#include "../include/ProcessControlBlock.h"

#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
using PCB = ProcessControlBlock;
using State = PCB::ProcessState;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename Exception, typename Action>
void expectThrow(Action action) {
    try {
        action();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error("Expected exception was not thrown");
}

void testLifecycleAndScheduling() {
    PCB pcb(1, "worker", 5, 2, 0, 3);
    check(pcb.getPid() == 1 && pcb.getParentPid() == 0 && pcb.getName() == "worker",
          "Process identity");
    check(pcb.getState() == State::New && pcb.getPriority() == 2, "Initial state");
    check(!pcb.getStartTime() && !pcb.getFinishTime() && !pcb.getExitCode() &&
          !pcb.getTurnaroundTime(), "Unset timestamps and exit code");
    pcb.setPriority(-1);
    check(pcb.getPriority() == -1, "Priority update");
    expectThrow<std::logic_error>([&] { pcb.setRunning(3); });
    pcb.setReady();
    pcb.updateWaitTime(2);
    expectThrow<std::invalid_argument>([&] { pcb.setRunning(4); });
    pcb.setRunning(5);
    pcb.resetTimeSlice(2);
    check(!pcb.consumeTimeSlice(), "First tick should not exhaust slice");
    check(pcb.consumeTimeSlice(), "Second tick should exhaust slice");
    expectThrow<std::logic_error>([&] { pcb.consumeTimeSlice(); });
    check(pcb.getCpuTime() == 2 && pcb.getRemainingTime() == 3, "CPU accounting");
    pcb.setReady();
    pcb.updateWaitTime(1);
    pcb.resetTimeSlice(3);
    pcb.setRunning(8);
    pcb.updateRuntime(3);
    check(pcb.isFinished() && pcb.isRunning(), "Scheduler explicitly terminates finished work");
    expectThrow<std::invalid_argument>([&] { pcb.updateRuntime(1); });
    expectThrow<std::invalid_argument>([&] { pcb.terminate(10); });
    pcb.terminate(11);
    check(pcb.isTerminated() && pcb.getExitCode() == 0, "Termination");
    check(pcb.getStartTime() == 5 && pcb.getFinishTime() == 11 &&
          pcb.getTurnaroundTime() == 8 && pcb.getWaitTime() == 3, "Timing statistics");
    expectThrow<std::logic_error>([&] { pcb.setReady(); });
    expectThrow<std::logic_error>([&] { pcb.setRunning(11); });
    expectThrow<std::logic_error>([&] { pcb.terminate(11); });
    expectThrow<std::logic_error>([&] { pcb.resetTimeSlice(1); });
}

void testBlockingAndSuspension() {
    PCB pcb(2, "io", 8);
    pcb.setReady();
    pcb.suspend();
    check(pcb.getSuspendedState() == State::Ready, "Save ready state");
    expectThrow<std::logic_error>([&] { pcb.wakeup(); });
    pcb.resume();
    check(pcb.isReady() && !pcb.getSuspendedState(), "Resume ready state");
    pcb.setRunning(0);
    expectThrow<std::logic_error>([&] { pcb.suspend(); });
    expectThrow<std::invalid_argument>([&] { pcb.setBlocked(""); });
    pcb.updateRuntime(1);
    pcb.setBlocked("waiting for disk");
    pcb.setWaitingResource(7);
    pcb.suspend();
    pcb.resume();
    check(pcb.isBlocked() && pcb.getBlockedReason() == "waiting for disk",
          "Resume must preserve blocking state");
    pcb.suspend();
    pcb.wakeup();
    check(pcb.isSuspended() && pcb.getSuspendedState() == State::Ready &&
          pcb.getBlockedReason().empty() && !pcb.getWaitingResource(),
          "Wakeup must not activate a suspended process");
    pcb.resume();
    pcb.setRunning(4);
    pcb.setBlocked("semaphore");
    pcb.wakeup();
    check(pcb.isReady(), "Normal wakeup");
    expectThrow<std::logic_error>([&] { pcb.resume(); });
    expectThrow<std::logic_error>([&] { pcb.setBlocked("invalid"); });
    pcb.terminate(4, -1);
    check(pcb.getExitCode() == -1 && pcb.getRemainingTime() == 7,
          "Forced termination preserves remaining work");
}

void testContextAndBookkeeping() {
    PCB pcb(3, "memory", 1);
    PCB::CPUContext context;
    check(pcb.restoreContext() == context, "Zero-initialized context");
    context.programCounter = 100;
    context.registers[15] = 42;
    context.stackPointer = 4096;
    pcb.saveContext(context);
    check(pcb.restoreContext() == context, "Context roundtrip");
    pcb.setMemoryInfo(1024, 4096);
    pcb.setPageTable({2, 4, 6});
    pcb.setSwapped(true);
    check(pcb.getMemoryBase() == 1024 && pcb.getMemorySize() == 4096 &&
          pcb.getPageTable().size() == 3 && pcb.isSwapped(), "Memory metadata");
    pcb.addResource(1);
    pcb.addResource(1);
    pcb.addChild(4);
    pcb.addChild(4);
    pcb.addOpenFile("data.txt");
    pcb.addOpenFile("data.txt");
    check(pcb.getHeldResources().size() == 1 && pcb.getChildren().size() == 1 &&
          pcb.getOpenFiles().size() == 1, "Unique bookkeeping entries");
    pcb.setWaitingResource(2);
    pcb.clearWaitingResource();
    check(!pcb.getWaitingResource(), "Clear resource wait");
    pcb.terminate(0);
    check(pcb.getMemorySize() == 4096 && pcb.getHeldResources().size() == 1,
          "Keep metadata for manager cleanup after termination");
    check(pcb.releaseResource(1) && !pcb.releaseResource(1), "Resource cleanup");
    check(pcb.removeChild(4) && !pcb.removeChild(4), "Child cleanup");
    check(pcb.removeOpenFile("data.txt") && !pcb.removeOpenFile("data.txt"), "File cleanup");
    pcb.clearMemoryInfo();
    check(pcb.getMemorySize() == 0 && pcb.getMemoryBase() == 0 &&
          pcb.getPageTable().empty() && !pcb.isSwapped(), "Memory cleanup");
    std::ostringstream output;
    pcb.print(output);
    check(output.str() == pcb.toString() + '\n' &&
          output.str().find("Terminated") != std::string::npos, "Printable state");
}

void testInvalidArgumentsAndBoundaries() {
    expectThrow<std::invalid_argument>([] { PCB(-1, "invalid", 1); });
    expectThrow<std::invalid_argument>([] { PCB(1, "", 1); });
    expectThrow<std::invalid_argument>([] { PCB(1, "invalid", -1); });
    expectThrow<std::invalid_argument>([] { PCB(1, "invalid", 1, 0, 1); });
    expectThrow<std::invalid_argument>([] { PCB(1, "invalid", 1, 0, -2); });
    expectThrow<std::invalid_argument>([] { PCB(1, "invalid", 1, 0, -1, -1); });
    PCB pcb(4, "validation", 2);
    expectThrow<std::invalid_argument>([&] { pcb.resetTimeSlice(0); });
    expectThrow<std::invalid_argument>([&] { pcb.addResource(-1); });
    expectThrow<std::invalid_argument>([&] { pcb.setWaitingResource(-1); });
    expectThrow<std::invalid_argument>([&] { pcb.addChild(4); });
    expectThrow<std::invalid_argument>([&] { pcb.addOpenFile(""); });
    expectThrow<std::invalid_argument>([&] { pcb.setMemoryInfo(0, 0); });
    expectThrow<std::invalid_argument>([&] {
        pcb.setMemoryInfo(std::numeric_limits<std::uint64_t>::max(), 2);
    });
    expectThrow<std::logic_error>([&] { pcb.updateRuntime(1); });
    expectThrow<std::logic_error>([&] { pcb.updateWaitTime(1); });
    pcb.setReady();
    expectThrow<std::invalid_argument>([&] { pcb.updateWaitTime(-1); });
    pcb.setRunning(0);
    expectThrow<std::logic_error>([&] { pcb.consumeTimeSlice(); });
    expectThrow<std::invalid_argument>([&] { pcb.updateRuntime(0); });
    expectThrow<std::invalid_argument>([&] { pcb.updateRuntime(-1); });
    expectThrow<std::invalid_argument>([&] { pcb.updateRuntime(3); });
    check(pcb.getCpuTime() == 0 && pcb.getRemainingTime() == 2,
          "Failed updates must not modify accounting");

    PCB zero(5, "zero", 0);
    zero.setReady();
    check(zero.isFinished(), "Zero-length process");
    expectThrow<std::logic_error>([&] { zero.setRunning(0); });
    zero.terminate(0);
    check(zero.getTurnaroundTime() == 0 && !zero.getStartTime(), "Unstarted termination");

    PCB limit(6, "clock-limit", 2, 0, -1, std::numeric_limits<int>::max());
    limit.setReady();
    expectThrow<std::overflow_error>([&] { limit.updateWaitTime(1); });
    limit.setRunning(std::numeric_limits<int>::max());
    expectThrow<std::overflow_error>([&] { limit.updateRuntime(1); });
    check(limit.getCpuTime() == 0 && limit.getRemainingTime() == 2,
          "Overflow must leave state unchanged");
}
}

int main() {
    try {
        testLifecycleAndScheduling();
        testBlockingAndSuspension();
        testContextAndBookkeeping();
        testInvalidArgumentsAndBoundaries();
        std::cout << "All PCB tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "PCB test failed: " << error.what() << '\n';
        return 1;
    }
}
