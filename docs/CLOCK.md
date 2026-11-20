# 全局模拟时钟 CLOCK

`include/CLOCK.h` 声明接口，`src/CLOCK.cpp` 实现。
这里的 CLOCK 是系统模拟时钟，不是内存管理中的 Clock 页面置换算法。

## 设计边界

- 系统主循环持有一个 `CLOCK` 实例，调度器等模块共享该实例，而不是各自创建时钟。
- 不使用单例或全局静态变量：可在测试中创建彼此独立的模拟系统。
- 时间采用非负 `int` tick，与 PCB 的 `setRunning()`、`terminate()` 等接口兼容。
- 不创建线程、不读取真实时间、不自动修改 PCB，也不直接执行 MLFQ 调度。
- 当前采用主动调用模型：主循环每轮代表一次时钟节拍，不需要操作系统信号或回调。
- 如需实时演示，可由界面/主循环添加等待；等待只影响演示速度，不改变模拟结果。

## 接口

| 接口 | 含义 |
| --- | --- |
| `CLOCK clock;` | 从时刻 0 开始 |
| `CLOCK clock(10);` | 从指定的非负时刻开始 |
| `clock.now()` | 只读当前时刻，不推进时间 |
| `clock.advance()` | 推进 1 tick，返回新时刻 |
| `clock.advance(n)` | 推进 n 个 tick，n 必须大于 0 |
| `clock.reset()` / `clock.reset(t)` | 开始一轮新模拟，重置为 0 或指定非负时刻 |

非法参数抛出 `std::invalid_argument`；超过 `int` 上限抛出 `std::overflow_error`。
检查失败不会修改时钟。`reset()` 不会重置任何 PCB；不能在正在运行的系统中单独将时钟归零。

## 与调度器的调用顺序

每轮处理区间 `[clock.now(), clock.now() + 1)`：

1. 处理当前时刻到达的作业、I/O 完成等事件。
2. 必要时由调度器选择进程，调用 `setRunning(clock.now())`。
3. 当前进程执行一个 tick，其他就绪进程分别累计一个 tick 等待时间。
4. 全局时钟只调用一次 `advance()`。
5. 在推进后的时刻处理运行完成、时间片耗尽；两者同时发生时优先处理运行完成。

CPU 空闲时也必须推进时间。可以逐 tick 推进；只有确认中间没有必须处理的事件时，
才可批量 `advance(n)` 跳到下一事件。`advance(n)` 本身不处理跳过区间内的任何事件。
整个模拟系统仍需协调检查时间上限，避免先修改 PCB 后才发现全局时钟已无法推进。

下面是单进程示例；尚未修改项目的 `main.cpp` 或实现 MLFQ：

```cpp
#include "../include/CLOCK.h"
#include "../include/PCB.h"

int main() {
    CLOCK clock;
    PCB pcb(1, "worker", 5);
    pcb.setReady();
    pcb.resetTimeSlice(2);
    pcb.setRunning(clock.now());

    while (!pcb.isFinished()) {
        bool expired = pcb.consumeTimeSlice();
        clock.advance();

        if (pcb.isFinished()) {
            pcb.terminate(clock.now());
        } else if (expired) {
            pcb.setReady();
            // 只有一个进程，立即重新调度；多进程时在此选择就绪队列中的进程。
            pcb.resetTimeSlice(2);
            pcb.setRunning(clock.now());
        }
    }
    pcb.print();
}
```

`consumeTimeSlice()` 已执行一个 tick，不能再为同一 tick 调用 `updateRuntime(1)`。
没有设置时间片时可使用 `updateRuntime(1)`。全局时钟必须由主循环唯一推进，
PCB 中的 `lastTime_` 只负责本进程的时间记录与校验，不是另一个全局时钟。

## 测试

```sh
cmake -S . -B build
cmake --build build --target format
cmake --build build --target format-check
cmake --build build -j
ctest --test-dir build --output-on-failure
```

`CLOCKTest` 覆盖初始时间、单步/批量推进、重置、非法参数、溢出、CPU 空闲，
以及多个 PCB 共享一个时钟时的运行与等待计时。
