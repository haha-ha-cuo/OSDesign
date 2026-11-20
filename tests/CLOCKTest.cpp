#include "../include/CLOCK.h"
#include "../include/PCB.h"

#include <iostream>
#include <limits>
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

void testClockOperations() {
    CLOCK clock;
    const CLOCK& readOnlyClock = clock;
    check(readOnlyClock.now() == 0, "Clock starts at zero");
    check(clock.advance() == 1 && clock.now() == 1, "Single tick");
    check(clock.advance(4) == 5, "Multiple ticks");
    clock.reset();
    check(clock.now() == 0, "Reset to zero");
    clock.reset(10);
    check(clock.now() == 10 && clock.advance() == 11, "Reset to a starting time");
    CLOCK other(3);
    check(other.now() == 3 && clock.now() == 11, "Separate simulation clocks are independent");
}

void testInvalidArgumentsAndOverflow() {
    expectThrow<std::invalid_argument>([] { CLOCK invalid(-1); });
    CLOCK clock(5);
    expectThrow<std::invalid_argument>([&] { clock.advance(0); });
    expectThrow<std::invalid_argument>([&] { clock.advance(-1); });
    expectThrow<std::invalid_argument>([&] { clock.reset(-1); });
    check(clock.now() == 5, "Rejected operations must not change time");

    clock.reset(std::numeric_limits<int>::max() - 1);
    expectThrow<std::overflow_error>([&] { clock.advance(2); });
    check(clock.now() == std::numeric_limits<int>::max() - 1, "Overflow leaves time unchanged");
    check(clock.advance() == std::numeric_limits<int>::max(), "Maximum representable time");
    expectThrow<std::overflow_error>([&] { clock.advance(); });
    check(clock.now() == std::numeric_limits<int>::max(), "Clock must not wrap around");
}

void testClockWithProcesses() {
    CLOCK clock;
    // CPU 空闲时仍能推进，直到时刻 3 有作业到达。
    clock.advance(3);
    PCB first(1, "first", 2, 0, -1, clock.now());
    PCB second(2, "second", 1, 0, -1, clock.now());
    first.setReady();
    second.setReady();
    first.resetTimeSlice(2);
    first.setRunning(clock.now());

    // 两个进程共享同一时钟；一个运行、另一个等待，不能把全局时间增加两次。
    check(!first.consumeTimeSlice(), "First tick of the time slice");
    second.updateWaitTime(1);
    clock.advance();
    check(clock.now() == 4 && second.getWaitTime() == 1, "One global tick for all processes");
    check(first.consumeTimeSlice(), "Time slice exhausted");
    second.updateWaitTime(1);
    clock.advance();
    first.terminate(clock.now());
    check(first.getFinishTime() == 5 && first.getTurnaroundTime() == 2, "First process timing");

    second.setRunning(clock.now());
    second.updateRuntime(1);
    clock.advance();
    second.terminate(clock.now());
    check(clock.now() == 6 && second.getStartTime() == 5 && second.getFinishTime() == 6 &&
              second.getWaitTime() == 2 && second.getTurnaroundTime() == 3,
          "Second process timing");
    check(first.getCpuTime() + second.getCpuTime() == clock.now() - 3,
          "CPU accounting matches elapsed non-idle time");
    clock.advance();
    check(first.getCpuTime() == 2 && second.getCpuTime() == 1,
          "Clock must not modify process state itself");
}
} // namespace

int main() {
    try {
        testClockOperations();
        testInvalidArgumentsAndOverflow();
        testClockWithProcesses();
        std::cout << "All CLOCK tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "CLOCK test failed: " << error.what() << '\n';
        return 1;
    }
}
