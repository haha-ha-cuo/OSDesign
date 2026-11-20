# 多级反馈队列调度（MLFQ）

`include/MLFQ.h` 声明接口，`src/MLFQ.cpp` 实现。
MLFQ 只负责「把就绪进程放进哪个队列、让谁运行、时间片用完后放回哪里」，
不执行 tick、不推进全局时钟，也不复制 PCB。

## 设计边界

- 进程表由调度器持有（`std::vector<std::unique_ptr<PCB>>`），各就绪队列只保存
  `PCB*`，保证 CPU 装载的进程与队列中的进程是同一个对象，不会出现悬空指针。
- CPU 必须绑定同一个 `CLOCK`，并且此后只由本调度器装载进程；`MLFQ` 每轮只调用
  一次 `clock_.advance()`。
- 不创建线程、不读取真实时间、不阻塞等待，采用「主循环每轮一个 tick」的主动推进模型。
- 暂不支持 I/O 阻塞/唤醒、挂起与多核；到达时间由 `addProcess()` 指定，进程到达前保持 `New`。

## 队列与规则

默认 5 个就绪队列，级别越高时间片越短、优先级越高：

| 队列 | 时间片（默认） | 说明 |
| --- | --- | --- |
| Q0 | 1 tick | 新进程入队；短作业通常在此完成 |
| Q1 | 2 tick | 时间片耗尽的进程降级到此 |
| Q2 | 4 tick | 再次耗尽继续下沉 |
| Q3 | 8 tick | 长作业 |
| Q4 | 16 tick | 最低级，不再下沉 |

调度规则：

1. 新进程进入 Q0；
2. 每个队列内部按先来先服务排队，进程获得该级时间片 `baseSlice * sliceGrowth^level`；
3. 时间片用完仍未结束：降一级放入队尾，时间片随之变长；
4. 被更高级别队列抢占：回到**原队列**队尾，不降级；
5. 更高级别队列非空时，可以抢占低级别队列中正在运行的进程（同级别不抢占）；
6. 可选周期性提升：每 `boostInterval` 个 tick，把等待中的低级别进程全部送回 Q0，
   防止长作业在低级别队列中饥饿（`boostInterval = 0` 表示关闭）。

时间片在每次装载时重新下发，被抢占的进程不继承上一轮剩余量；这样每个执行片段
的时间片语义清晰，也便于统计。

## 属性与内部结构

| 成员 | 含义 |
| --- | --- |
| `options_` | 级数、基础时间片、时间片倍数、是否抢占、提升周期 |
| `processes_` | 进程表，拥有 PCB；`addProcess()` 返回其引用 |
| `queues_` | 每级一个 `std::deque<PCB*>`，队首是下一个候选进程 |
| `levelByPid_` | PID 到当前队列级别的映射；进程终止后移除 |
| `timeline_` | 甘特图片段，记录 PID、名称、起止时刻与运行时的队列级别 |
| `busyTicks_` / `totalTicks_` | 统计 CPU 利用率与吞吐量 |
| `running_` | 当前装载的 PCB；空闲时为空指针 |
| `nextBoostTick_` | 下一次周期性提升的时刻 |

## 一个 tick 的执行顺序

1. 处理当前时刻到达的作业（`New -> Ready`），进入 Q0；
2. 若更高级别队列非空，抢占当前进程：`release()` 使其 `Running -> Ready` 并回到原队列；
3. CPU 空闲时取第一个非空队列的队首进程，按该级时间片 `resetTimeSlice()` 后 `dispatch()`；
4. `CPU::tick()` 执行一个 tick，登记甘特图片段；
5. 其余就绪进程累计 1 tick 等待时间；
6. 全局时钟推进一次；
7. 完成优先处理：`terminate()` + `release()`；否则时间片耗尽时 `release()` 并降级；
8. 处理周期性提升（若启用）。

CPU 已调用 `updateRuntime(1)`，调度器不能再为同一 tick 重复扣减；
完成与时间片耗尽同一 tick 发生时优先处理完成，与 `docs/CPU.md` 的约定一致。

## 接口

| 接口 | 行为 |
| --- | --- |
| `MLFQ(clock, cpu, options)` | 创建调度器，CPU 必须与时钟绑定 |
| `addProcess(pid, name, burst, priority, arrival)` | 登记作业，PID 唯一、服务时间为正 |
| `step()` | 推进一个系统 tick；结束后可观察队列与统计 |
| `run(maxTicks)` | 循环到全部结束；超过上限抛出 `std::overflow_error` |
| `isFinished()` | 是否所有进程都已终止 |
| `getTimeline()` | 甘特图：PID、名称、起止时刻、队列级别 |
| `getMetrics()` | 平均周转/等待/响应时间、CPU 利用率、吞吐量 |
| `getQueueSize(level)` | 第 level 级队列中等待的进程数（不含运行中的） |
| `getLevelOf(pid)` | 进程当前级别；未登记或已结束返回 `-1` |
| `getTimeSlice(level)` | 第 level 级的时间片 |
| `getLevels()` / `getOptions()` | 级数与当前配置 |

非法参数抛出 `std::invalid_argument`（级数小于 1、时间片非正、倍率小于 1、
提升周期为负、时间片溢出 `int`、PID 重复、服务时间非正、级别越界）；
提升时刻越界或超过 tick 上限抛出 `std::overflow_error`；
在无进程可运行或 CPU 被外部改动时继续推进抛出 `std::logic_error`。

## 示例

```cpp
CLOCK clock;
CPU cpu(0, clock);
MLFQ::Options options; // 5 级，时间片 1/2/4/8/16，抢占，关闭提升
options.boostInterval = 12;
MLFQ scheduler(clock, cpu, options);

scheduler.addProcess(1, "P1", 7, 0, 0);
scheduler.addProcess(2, "P2", 4, 0, 2);
scheduler.run();

for (const MLFQ::TimelineEntry& entry : scheduler.getTimeline()) {
    std::cout << '[' << entry.startTime << ',' << entry.endTime << ") " << entry.name
              << " Q" << entry.queueLevel << '\n';
}
```

也可以手动逐 tick 调用 `step()`，并在任意时刻用 `getLevelOf()`、`getQueueSize()`
观察进程下沉与提升的情况。

## 统计指标

- 周转时间 = 完成时刻 - 到达时刻；
- 等待时间 = 就绪队列中累计等待的 tick 数（不含阻塞，本模块不模拟阻塞）；
- 响应时间 = 首次开始运行 - 到达时刻；
- CPU 利用率 = 忙碌 tick / 总 tick；
- 吞吐量 = 完成进程数 / 总 tick。

`getMetrics()` 只统计已终止的进程，便于单步调试时查看当前进度。

## 复杂度

- 选择进程：最坏 O(队列级数)，空队列会被跳过；
- 入队/出队：O(1)；
- 周期性提升：一次 O(就绪进程数)；
- 进程表登记与校验：O(进程数)。

默认 5 级、进程数为课程规模时开销可以忽略。

## 与其它模块的关系

- `CLOCK`：唯一的时间来源，由主循环或本模块的 `step()` 推进；
- `CPU`：执行一个 tick，返回 `Running`/`TimeSliceExpired`/`Finished`；调度器负责装载与卸载；
- `PCB`：保存状态、剩余时间、时间片与统计；队列只引用它，不复制；
- 进程创建/撤销与 I/O 阻塞留待进程管理模块，接入后可在唤醒时按约定重新入队。

## 测试

`tests/MLFQTest.cpp` 覆盖级内轮转、逐级降级、被抢占不降级、时间片倍增、
周期性提升、单级队列退化为轮转、空闲 tick 以及各类非法参数：

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```
