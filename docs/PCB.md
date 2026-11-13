# 进程控制块（PCB）

`../include/PCB.h` 定义 PCB，`../src/PCB.cpp` 实现。
PCB 保存进程信息，不负责创建真实进程、执行调度、分配内存或进行文件 I/O。

## 属性

| 类别 | 内容 |
| --- | --- |
| 标识 | PID、父 PID（`-1` 表示无父进程）、名称、优先级、退出码 |
| 状态 | 新建、就绪、运行、阻塞、挂起、终止；阻塞原因及挂起前状态 |
| 调度 | 到达时间、总运行预算、剩余时间、累计 CPU/就绪队列等待时间、时间片 |
| 统计 | 首次运行时间、终止时间、周转时间；未发生的事件返回 `std::nullopt` |
| CPU 上下文 | 程序计数器、16 个模拟寄存器、栈指针 |
| 内存 | 起始地址、大小、页表（下标为逻辑页，值为物理页框号）、换出标记 |
| 资源 | 独立资源实例 ID、等待资源 ID、子进程 PID、打开的文件名称 |

PID 唯一性、父子关系双向维护、资源所有权由对应管理器保证。
优先级允许任意整数，数值越大还是越小优先由调度算法决定。
页表、换出标记由内存管理器维护，PCB 不校验真实映射或自动换入换出。
文件名称是简化记录，不表示真实文件描述符，不支持同一文件的多个打开实例。

## 状态转换

- `New -> Ready`：`setReady()`。
- `Ready -> Running`：`setRunning(currentTime)`，首次调用记录开始时间。
- `Running -> Ready`：`setReady()`，用于抢占；已就绪时重复调用无副作用。
- `Running -> Blocked`：`setBlocked(reason)`。
- `Blocked -> Ready`：`wakeup()`，清除阻塞原因及等待资源。
- `Ready/Blocked -> Suspended`：`suspend()`；`resume()` 恢复挂起前状态。
- 挂起期间收到 `wakeup()`：仍保持挂起，但恢复目标改为 `Ready`。
- 任意未终止状态可 `terminate(currentTime, exitCode)`，支持强制撤销。

非法转换抛出 `std::logic_error`，非法参数抛出 `std::invalid_argument`，
计时溢出抛出 `std::overflow_error`。运行中的进程应先抢占到就绪态再挂起。
终止不会清空内存、资源及文件记录，管理器应据此回收，再清除相应记录。

## 计时约定与示例

所有调度时间以非负 `int` tick 表示。调度器提供当前时刻，并调用
`updateRuntime(ticks)` 累计运行时间；该方法不读取真实时钟。
`updateWaitTime(ticks)` 只累计就绪队列等待，同时推进该 PCB 的时钟。
调度器需要自行按时钟推进记录等待时间；`setRunning()` 不自动累计等待。
显式传入的当前时刻不得早于该进程最近一次调度或计时结束时刻。

```cpp
ProcessControlBlock pcb(1, "worker", 3); // PID、名称、运行预算
pcb.setReady();
pcb.resetTimeSlice(2);
pcb.setRunning(0);
pcb.consumeTimeSlice(); // 执行 1 tick，返回 false
pcb.consumeTimeSlice(); // 再执行 1 tick，返回 true（时间片耗尽）
pcb.setReady();
pcb.resetTimeSlice(2);
pcb.setRunning(2);
pcb.updateRuntime(1);
if (pcb.isFinished()) pcb.terminate(3);
pcb.print();
```

没有设置时间片时，`updateRuntime()` 可用于 FCFS/SJF；配置时间片后不能超过
剩余时间片运行。`consumeTimeSlice()` 本身执行一个 tick，不要再为同一 tick 调用
`updateRuntime()`。时间片耗尽或运行预算归零不会自动切换状态，由调度器处理；
进程重新获得 CPU 时由调度器重置时间片。运行预算为零的进程可直接终止。
`isFinished()` 表示运行预算耗尽，`isTerminated()` 表示已经终止，两者不同。

## 测试

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
