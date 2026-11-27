#include "../include/Simulation.h"

#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testDynamicProcesses() {
    Simulation simulation;
    const auto idle = simulation.step();
    check(idle.result == CPU::TickResult::Idle && idle.process == nullptr && simulation.time() == 1,
          "An idle clock signal should still advance time");

    const auto& longJob = simulation.addProcess("long", 5);
    check(longJob.pcb.getPid() == 1 && longJob.pcb.getArrivalTime() == 1 && longJob.pcb.isReady(),
          "A new process should arrive in the ready queue at the current time");
    const auto first = simulation.step();
    check(first.dispatched && first.process == &longJob &&
              first.result == CPU::TickResult::Running &&
              simulation.cpu().getCurrentProcess() == &longJob.pcb,
          "The first signal should dispatch and run the ready process");

    const auto& shortJob = simulation.addProcess("short", 1);
    check(shortJob.pcb.getPid() == 2 && shortJob.pcb.getArrivalTime() == 2 &&
              simulation.cpu().getCurrentProcess() == &longJob.pcb,
          "Adding a process while the CPU is busy should preserve its current PCB");
    simulation.step();
    simulation.step();
    const auto expired = simulation.step();
    check(expired.result == CPU::TickResult::TimeSliceExpired && expired.queueLevel == 1 &&
              longJob.queueLevel == 2 && longJob.pcb.isReady() &&
              longJob.pcb.restoreContext().registers[0] == 4 && simulation.cpu().isIdle(),
          "A time-slice expiration should save context and move the process down one queue");

    const auto completed = simulation.step();
    check(completed.dispatched && completed.process == &shortJob &&
              completed.result == CPU::TickResult::Finished && shortJob.pcb.getFinishTime() == 6 &&
              shortJob.pcb.getWaitTime() == 3,
          "A one-tick job should be observable even if it starts and finishes in one signal");
    const auto resumed = simulation.step();
    check(resumed.process == &longJob && resumed.dispatched && resumed.previousR0 == 4 &&
              resumed.result == CPU::TickResult::Finished && longJob.pcb.getFinishTime() == 7 &&
              longJob.pcb.restoreContext().registers[0] == 5 && simulation.cpu().isIdle(),
          "The demoted process should resume with its preserved context and finish");
}

void testHigherQueuePreemption() {
    Simulation simulation;
    const auto& longJob = simulation.addProcess("long", 10);
    for (int tick = 0; tick < 5; ++tick) {
        simulation.step();
    }
    check(longJob.queueLevel == 2 && longJob.pcb.getCpuTime() == 5 &&
              simulation.cpu().getCurrentProcess() == &longJob.pcb,
          "The long job should be running in Q2 before a higher-priority arrival");

    const auto& shortJob = simulation.addProcess("short", 1);
    const auto preempting = simulation.step();
    check(preempting.dispatched && preempting.process == &shortJob && preempting.queueLevel == 1 &&
              preempting.result == CPU::TickResult::Finished && shortJob.pcb.getFinishTime() == 6 &&
              shortJob.pcb.getWaitTime() == 0,
          "A Q1 arrival should preempt Q2 at the next tick boundary");
    check(longJob.pcb.isReady() && longJob.queueLevel == 2 &&
              longJob.pcb.restoreContext().registers[0] == 5 && longJob.pcb.getWaitTime() == 1,
          "Preemption should preserve the queue level, save context and count waiting time");

    const auto resumed = simulation.step();
    check(resumed.dispatched && resumed.process == &longJob && resumed.queueLevel == 2 &&
              resumed.previousR0 == 5 && longJob.pcb.getRemainingTimeSlice() == 3,
          "The preempted job should resume in Q2 with saved context and a fresh time slice");
    simulation.step();
    simulation.step();
    const auto expired = simulation.step();
    check(expired.result == CPU::TickResult::TimeSliceExpired && longJob.queueLevel == 3,
          "Only exhausting the new time slice should demote the preempted job");
    const auto finished = simulation.step();
    check(finished.result == CPU::TickResult::Finished && longJob.pcb.getFinishTime() == 11 &&
              longJob.pcb.restoreContext().registers[0] == 10 &&
              longJob.pcb.getTurnaroundTime() ==
                  longJob.pcb.getCpuTime() + longJob.pcb.getWaitTime(),
          "Preemption must preserve CPU, wait-time and context accounting through completion");
    check(simulation.step().result == CPU::TickResult::Idle,
          "Completed jobs must leave no runnable entries");
    const auto& laterJob = simulation.addProcess("later", 1);
    check(simulation.step().process == &laterJob && laterJob.pcb.isTerminated(),
          "New arrivals should still run after all earlier jobs have completed");
}

void testLowestQueue() {
    Simulation simulation;
    const auto& job = simulation.addProcess("long", 25);
    for (int tick = 1; tick <= 25; ++tick) {
        const auto result = simulation.step();
        if (tick == 16 || tick == 20 || tick == 24) {
            check(result.result == CPU::TickResult::TimeSliceExpired &&
                      job.queueLevel == MLFQ::kLevels,
                  "Repeated expiration in the lowest queue must keep the job at Q5");
        }
    }
    check(job.pcb.isTerminated() && job.pcb.getFinishTime() == 25 &&
              job.pcb.restoreContext().registers[0] == 25 && simulation.cpu().isIdle(),
          "A job passing through every queue should finish without losing work");
}

void testInvalidWorkload() {
    Simulation simulation;
    try {
        simulation.addProcess("empty", 0);
        throw std::runtime_error("Creating a zero-tick process should fail");
    } catch (const std::invalid_argument&) {
        check(simulation.processes().empty() && simulation.addProcess("valid", 1).pcb.getPid() == 1,
              "Rejecting a process should leave the table and PID allocation intact");
    }
}
} // namespace

int main() {
    try {
        testDynamicProcesses();
        testHigherQueuePreemption();
        testLowestQueue();
        testInvalidWorkload();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
