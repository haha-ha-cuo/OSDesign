# 多级反馈队列调度（MLFQ）

`include/MLFQ.h` 声明接口，`src/MLFQ.cpp` 实现。
MLFQ 只负责「把就绪进程放进哪个队列、让谁运行、时间片用完后放回哪里」，
不执行 tick、不推进全局时钟，也不拥有 PCB。

## 设计边界

- 队列只保存 `PCB*`，PCB 本体由进程表持有，避免复制进程状态、避免 CPU 指向另一份副本；
- 时间片长度与全局时钟由调用方掌握：`MLFQ` 不引用 `CLOCK`，只在派发时调用
  `PCB::resetTimeSlice()`，每轮 `advance()` 由主循环执行一次；
- 不创建线程、不读取真实时间、不阻塞等待，采用「主循环每轮一个 tick」的主动推进模型；
- 不支持 I/O 阻塞/唤醒、挂起与多核；这些属于进程管理/系统模块的职责。

## 队列与规则

五级反馈队列，1 级优先级最高、5 级最低，同级按 FCFS 派发：

| 队列 | 说明 |
| --- | --- |
| 1 | 新进程默认进入；CPU 空闲时最先被派发 |
| 2 | 在 1 级用完时间片后降级到此 |
| 3 | 再次耗尽继续下沉 |
| 4 | 长作业 |
| 5 | 最低级，不再继续下沉 |

调度规则：

1. `receivePCB(pcb, level = 1)` 把进程放进指定队列；默认进入 1 级；
2. `ProcessShifter()` 在 CPU 空闲时，从 1 级开始找第一个非空队列，取队首派发，
   并按 `timePiece` 重新设置时间片；
3. 时间片耗尽且未完成：调用方 `release()` 后调用 `TimeSliceExpired(pcb)`，
   进程自动降一级并排到队尾；已在 5 级则留在原级；
4. 高级别队列出现等待进程时，可抢占低级别队列中正在运行的进程：
   调用方先 `release()`，再调用 `Preempted(pcb)`，进程回到**原级别**队尾，不降级；
5. `BoostQueues()` 把所有等待进程按级别从高到低依次送回 1 级队列，
   用于周期性提升、防止低级别队列饥饿（调用周期由主循环决定）。

所有队列共用同一个 `timePiece`；如需「时间片随级别翻倍」，可在调用方按级别
`resetTimeSlice()`，或后续扩展为按级别配置。

## 属性与内部结构

| 成员 | 含义 |
| --- | --- |
| `Ready_processes1` … `Ready_processes5` | 五级就绪队列，`std::queue<PCB*>` |
| `timePiece` | 每次派发给进程的时间片长度，必须为正 |
| `levelByPid_` | 记录**排队中**进程的级别；派发后移除 |
| `dispatchedPid_` / `dispatchedLevel_` | 最近一次 `ProcessShifter()` 派发的进程及其级别 |
| `cpu_` | 被派发的 CPU 引用，不拥有 |

`getLevelOf(pid)` 同时覆盖「排队中」与「正在运行」两种状态：
运行中的进程从 `dispatchedPid_` 读级别，排队中的进程从 `levelByPid_` 读，
其余情况返回 `-1`。进程终止后应调用 `ProcessFinished()` 清除记录再销毁 PCB。

## 调用顺序

主循环每轮处理区间 `[clock.now(), clock.now() + 1)`：

1. 新进程到达：`mlfq.receivePCB(pcb)`（`New` 会自动转成 `Ready`）；
2. 判断抢占：`mlfq.ShouldPreempt(pcb)` 为真时，`cpu.release()` 后调用
   `mlfq.Preempted(pcb)`；
3. CPU 空闲：`mlfq.ProcessShifter()` 派发最高优先级非空队列的队首；
4. `cpu.tick()` 执行一个 tick；
5. 全局时钟推进一次；
6. 处理结果：
   - `Finished`：`pcb.terminate(clock.now())`、`cpu.release()`、`mlfq.ProcessFinished(pcb)`；
   - `TimeSliceExpired`：`cpu.release()`、`mlfq.TimeSliceExpired(pcb)`；
7. 需要防止饥饿时，定期调用 `mlfq.BoostQueues()`。

CPU 已调用 `updateRuntime(1)`，调用方不能再为同一 tick 重复扣减；完成与时间片
耗尽同时发生时优先处理完成，与 `docs/CPU.md` 的约定一致。

```cpp
// table 是调用方的进程表：PID -> PCB*，PCB 由进程表持有。
while (true) {
    if (cpu.isIdle() && !mlfq.ProcessShifter()) {
        break; // 所有队列为空，调度结束
    }
    PCB* running = table.at(cpu.getCurrentProcess()->getPid());
    const CPU::TickResult result = cpu.tick();
    clock.advance();
    if (result == CPU::TickResult::Finished) {
        running->terminate(clock.now());
        cpu.release();
        mlfq.ProcessFinished(*running);
    } else if (result == CPU::TickResult::TimeSliceExpired) {
        cpu.release();
        mlfq.TimeSliceExpired(*running);
    }
}
```

## 接口

| 接口 | 行为 |
| --- | --- |
| `MLFQ(cpu, timePiece = 16)` | 创建调度器，CPU 必须比它活得久 |
| `receivePCB(pcb, level = 1)` | 入队；`New` 自动转 `Ready`，并校验进程可运行 |
| `ProcessShifter()` | CPU 空闲时派发；返回是否发生派发 |
| `TimeSliceExpired(pcb)` | 时间片耗尽后降一级重新入队 |
| `Preempted(pcb)` | 被抢占后回到原级别队尾 |
| `ProcessFinished(pcb)` | 清除派发记录与级别信息 |
| `ShouldPreempt(running)` | 是否有更高级别队列等待 |
| `BoostQueues()` | 把所有等待进程提回 1 级 |
| `getLevelOf(pid)` | 当前级别；未登记或已结束返回 `-1` |
| `getQueueSize(level)` | 某级队列等待的进程数 |
| `getReadyCount()` | 所有队列等待的进程总数 |

## 异常约定

- 非法参数抛出 `std::invalid_argument`：`timePiece` 非正、级别不在 `[1, 5]`；
- 非法状态抛出 `std::logic_error`：向已终止、正在运行、已在本类队列中或已完成的进程
  再次入队；对非派发进程调用 `TimeSliceExpired()` / `Preempted()`；未 `release()`
  就重新入队；队列中出现空指针或不可运行的进程。

## 复杂度

- 派发：最坏 O(队列级数)，空队列会被跳过；
- 入队/出队：O(1)（`std::queue`）；
- 周期性提升：一次 O(就绪进程数)；
- 级别查询：平均 O(1)（哈希表）。

## 与其它模块的关系

- `CPU`：执行一个 tick，返回 `Running`/`TimeSliceExpired`/`Finished`；
  调度器只负责装载，不直接调用 `updateRuntime()`；
- `PCB`：提供状态、剩余时间、时间片与开始/完成时间；队列只引用它；
- `CLOCK`：不在本类中出现，由主循环统一推进，保证所有模块共享同一时间基准；
- 进程创建/撤销、I/O 阻塞等由后续的进程管理模块负责，唤醒后调用 `receivePCB()` 即可回到队列。

## 测试

`tests/MLFQTest.cpp` 覆盖入队校验、派发顺序、派发失败保留队首、时间片耗尽降级、
最低级不再下沉、抢占保持原级别、同级不抢占、周期性提升与完整的多进程执行序列：

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```
