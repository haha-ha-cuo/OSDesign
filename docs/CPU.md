# CPU 模块

`include/CPU.h` 声明接口，`src/CPU.cpp` 实现。一个 CPU 对象代表一个模拟核心。

## 职责与状态

- `id_`：非负核心编号，唯一性由系统管理器保证。
- `clock_`：共享全局时钟的只读引用。CPU 不创建私有时钟、不调用 `advance()`。
- `currentProcess_`：当前 PCB 的非拥有指针，空指针表示空闲。
- `context_`：当前 CPU 寄存器上下文；装载时恢复，卸载时保存。
- `nextTickTime_`：下一个可以执行的时刻，防止同一时刻重复执行或漏掉节拍。

`getStatus()` 返回公开的强类型枚举 `CPU::Status::FREE/OCCUPIED`。
状态由是否装载 PCB 推导，不额外存储一个容易与 PCB 指针不同步的状态变量。
`getId()` 等查询方法为 `const`；CPU 不可复制，避免复制出引用同一运行进程的核心。

CPU 只执行调度器选好的进程，不实现 FCFS/MLFQ、不决定进程进入哪个队列、
不自动终止进程。`tick()` 只模拟一个 tick 的 CPU 时间，不假定一个 tick 等于一条指令，
寄存器由调用方通过 `setContext()` 更新。

## 接口

| 接口 | 行为 |
| --- | --- |
| `CPU cpu(0, clock)` | 创建核心，时钟必须比 CPU 活得更久 |
| `dispatch(pcb)` | 将有剩余工作的 Ready 进程置为 Running，并恢复上下文 |
| `tick()` | 执行一个 tick，返回 Idle、Running、TimeSliceExpired 或 Finished |
| `release()` | 保存上下文并卸载，返回原 PCB 指针；空闲时返回空指针 |
| `getContext()` / `setContext(context)` | 读取 CPU 上下文 / 修改运行中的 CPU 上下文 |
| `getCurrentProcess()` | 只读访问当前 PCB，不转移所有权 |

一次 `tick()` 模拟区间 `[clock.now(), clock.now() + 1)`，不推进全局时间。
调用后主循环应为其他就绪进程累计等待时间，再统一推进时钟一次。
时间片和运行预算同时耗尽时优先返回 `Finished`。
收到 `Finished` 或 `TimeSliceExpired` 后必须处理结果，不能继续执行该进程。

`release()` 对仍在运行且未完成的进程执行 `Running -> Ready`，表示抢占；
已经终止的进程保持原状态。运行预算耗尽的进程必须先调用
`terminate(clock.now())`，再卸载。执行完一个 tick 后必须先推进时钟，才能卸载。
非法状态/节拍顺序抛出 `std::logic_error`；时钟到达 `int` 上限时执行 tick 抛出
`std::overflow_error`，不会修改 PCB 的运行时间。

## 最小时间片轮转示例

```cpp
CLOCK clock;
PCB pcb(1, "worker", 5);
CPU cpu(0, clock);
pcb.setReady();

while (!pcb.isTerminated()) {
    if (cpu.isIdle()) {
        pcb.resetTimeSlice(2);
        cpu.dispatch(pcb);
    }

    auto result = cpu.tick();
    // 多进程时在此为其他就绪进程分别 updateWaitTime(1)。
    clock.advance(); // 每个系统 tick 只能统一推进一次

    if (result == CPU::TickResult::Finished) {
        pcb.terminate(clock.now());
        cpu.release();
    } else if (result == CPU::TickResult::TimeSliceExpired) {
        cpu.release(); // PCB 回到 Ready，由调度器决定后续入队和降级
    }
}
```

CPU 已调用 `updateRuntime(1)`，主循环不能再为同一进程、同一 tick 调用
`updateRuntime(1)`。强制终止时，调用方应在节拍边界
修改 PCB 状态，再立即 `release()`，保证 CPU 与进程状态一致。
空闲 CPU 的 `tick()` 返回 `Idle`，主循环仍应推进全局时间，以处理未来事件。

## PCB 生命周期与 MLFQ 对接

CPU 不复制、销毁 PCB，也不拥有其内存。PCB 必须在卸载前保持存活且地址不变。
装载期间的寄存器以 CPU 中的上下文为准，不要直接修改 PCB 保存的旧上下文。

目前 MLFQ 使用 `std::queue<PCB>`。不能 `dispatch(queue.front())` 后立刻 `pop()`，
否则 CPU 的 PCB 指针会悬空。后续推荐由进程表统一管理 PCB，队列仅存 PID 或稳定的
PCB 指针；不能通过在队列间复制 PCB 来代替同一个进程。此次未实现或改动 MLFQ 调度逻辑。

主循环统一推进全局时钟后，再决定下一个节拍执行哪个核心上的进程。
即使扩展为多个 CPU，也必须先处理所有核心的当前 tick，再将全局时间推进一次。
重新开始模拟时需要一并重建 CPU、PCB 和时钟，不能只将时钟归零。

`tests/CPUTest.cpp` 覆盖装载、抢占、完成、上下文切换、时间片、重复/跳过节拍和溢出。
