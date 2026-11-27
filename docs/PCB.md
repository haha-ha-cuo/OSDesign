# 进程控制块（PCB）

`include/PCB.h` 定义 PCB，`src/PCB.cpp` 实现。
PCB 保存进程信息，不负责创建真实进程、执行调度、分配内存或进行文件 I/O。
当前仅保留 CPU、MLFQ、优先级调度和界面实际使用的数据及接口，后续按需求增量扩展。

## 属性

| 类别 | 内容 |
| --- | --- |
| 标识 | PID、名称、优先级 |
| 状态 | 新建、就绪、运行、终止 |
| 调度 | 到达时间、总运行预算、剩余时间、累计 CPU/就绪队列等待时间、时间片 |
| 统计 | 首次运行时间、终止时间、周转时间；未发生的事件返回 `std::nullopt` |
| CPU 上下文 | 16 个模拟寄存器，用于保存和恢复上下文 |

构造接口为 `PCB(pid, name, burstTime, priority = 0, arrivalTime = 0)`。
PID 唯一性由创建进程的模块保证。
优先级允许任意整数，数值越大还是越小优先由调度算法决定。

## 状态转换

- `New -> Ready`：`setReady()`。
- `Ready -> Running`：`setRunning(currentTime)`，首次调用记录开始时间。
- `Running -> Ready`：`setReady()`，用于抢占；已就绪时重复调用无副作用。
- 任意未终止状态可 `terminate(currentTime)`。

非法转换抛出 `std::logic_error`，非法参数抛出 `std::invalid_argument`，
计时溢出抛出 `std::overflow_error`。

## 计时约定与示例

所有调度时间以非负 `int` tick 表示。调度器提供当前时刻，并调用
`updateRuntime(ticks)` 累计运行时间；该方法不读取真实时钟。
`updateWaitTime(ticks)` 只累计就绪队列等待，同时推进该 PCB 的时钟。
调度器需要自行按时钟推进记录等待时间；`setRunning()` 不自动累计等待。
显式传入的当前时刻不得早于该进程最近一次调度或计时结束时刻。

```cpp
PCB pcb(1, "worker", 3); // PID、名称、运行预算
pcb.setReady();
pcb.resetTimeSlice(2);
pcb.setRunning(0);
pcb.updateRuntime(1); // 执行 1 tick，剩余时间片为 1
pcb.updateRuntime(1); // 再执行 1 tick，剩余时间片为 0
pcb.setReady();
pcb.resetTimeSlice(2);
pcb.setRunning(2);
pcb.updateRuntime(1);
if (pcb.isFinished()) pcb.terminate(3);
```

没有设置时间片时，`updateRuntime()` 只扣减运行预算；配置时间片后不能超过
剩余时间片运行。时间片耗尽或运行预算归零不会自动切换状态，由调度器处理；
进程重新获得 CPU 时由调度器重置时间片。运行预算为零的进程可直接终止。
`isFinished()` 表示运行预算耗尽，`isTerminated()` 表示已经终止，两者不同。

## 测试

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
