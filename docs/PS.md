# 优先级调度（PS）

`include/PS.h` 声明接口，`src/PS.cpp` 实现。
PS 只负责「从就绪进程中选择优先级最高者运行」，不执行 tick、不推进全局时钟，
也不复制 PCB。

## 设计边界

- 进程表由调度器持有（`std::vector<std::unique_ptr<PCB>>`），就绪进程通过
  `PCB::isReady()` 判定，不额外维护就绪链表；`addProcess()` 返回 PCB 引用，
  测试可直接断言开始/完成时间。
- CPU 必须绑定同一个 `CLOCK`，并且此后只由本调度器装载进程；`PS` 每轮只调用
  一次 `clock_.advance()`。
- 不创建线程、不读取真实时间，采用「主循环每轮一个 tick」的主动推进模型。
- 不设置时间片，CPU 正常只会返回 `Running` 或 `Finished`；暂不支持 I/O 阻塞、
  挂起与多核。

## 优先级约定

优先级允许任意整数，**数值越小优先级越高**（与 Unix nice 值语义一致）。
排序键依次为：

1. 优先级（数值小者优先）；
2. 到达时间（先到者优先，即同优先级按先来先服务 FCFS）；
3. PID（升序，保证同一组输入的结果可复现）。

## 调度规则

- **抢占式**（`preemptive = true`）：新进程到达时如果优先级高于当前运行进程，
  立即剥夺 CPU，被抢占的进程仍是 `Ready`，下一轮重新参与比较；
- **非抢占式**（`preemptive = false`）：新进程只进入就绪集合，等当前进程运行结束
  才重新选择；
- **老化**（`agingInterval > 0`）：每经过 `agingInterval` 个 tick，所有**等待中**的
  就绪进程优先级提升一级（数值减 1），下限为 `lowestPriority`。新到达的进程没有
  积累等待时间，不会同步提升，因此长期等待的进程最终能压过不断到来的高优先级作业。

老化只作用于 `Ready` 进程，运行中的进程不会被自我提升。

## 属性与内部结构

| 成员 | 含义 |
| --- | --- |
| `options_` | 是否抢占、老化周期、优先级下限 |
| `processes_` | 进程表，拥有 PCB；按登记顺序保存 |
| `timeline_` | 甘特图片段：PID、名称、起止时刻 |
| `busyTicks_` / `totalTicks_` | 统计 CPU 利用率与吞吐量 |
| `running_` | 当前装载的 PCB；空闲时为空指针 |
| `nextAgingTick_` | 下一次老化时刻；老化关闭时不使用 |

## 一个 tick 的执行顺序

1. 处理当前时刻到达的作业（`New -> Ready`），使其参与本轮比较；
2. 抢占式下，若存在比当前运行进程更优的就绪进程，`release()` 当前进程；
3. CPU 空闲时 `pickNext()` 选出最优就绪进程并 `dispatch()`；
4. `CPU::tick()` 执行一个 tick，登记甘特图片段；
5. 其余就绪进程累计 1 tick 等待时间；
6. 全局时钟推进一次；
7. 运行完成则 `terminate()` + `release()`；若外部设置了时间片而出现时间片耗尽，
   只把进程放回就绪集合，不做降级；
8. 执行老化（若启用）。

## 接口

| 接口 | 行为 |
| --- | --- |
| `PS(clock, cpu, options)` | 创建调度器，CPU 必须与时钟绑定 |
| `addProcess(pid, name, burst, priority, arrival)` | 登记作业，PID 唯一、服务时间为正 |
| `step()` | 推进一个系统 tick |
| `run(maxTicks)` | 循环到全部结束；超过上限抛出 `std::overflow_error` |
| `isFinished()` | 是否所有进程都已终止 |
| `getTimeline()` | 甘特图：PID、名称、起止时刻 |
| `getMetrics()` | 平均周转/等待/响应时间、CPU 利用率、吞吐量 |
| `getOptions()` | 当前配置 |

非法参数抛出 `std::invalid_argument`（老化周期为负、PID 重复、服务时间非正、
tick 上限非正）；老化时刻或 tick 上限越界抛出 `std::overflow_error`；
在无进程可运行或 CPU 被外部改动时继续推进抛出 `std::logic_error`。

## 示例

```cpp
CLOCK clock;
CPU cpu(0, clock);
PS::Options options;
options.preemptive = true;
options.agingInterval = 6;
PS scheduler(clock, cpu, options);

scheduler.addProcess(1, "P1", 7, 2, 0);
scheduler.addProcess(2, "P2", 4, 1, 2);
scheduler.run();

for (const PS::TimelineEntry& entry : scheduler.getTimeline()) {
    std::cout << '[' << entry.startTime << ',' << entry.endTime << ") " << entry.name << '\n';
}
```

非抢占式只需 `options.preemptive = false`；关闭老化使用 `agingInterval = 0`。

## 统计指标

- 周转时间 = 完成时刻 - 到达时刻；
- 等待时间 = 就绪队列中累计等待的 tick 数（本模块不模拟阻塞）；
- 响应时间 = 首次开始运行 - 到达时刻；
- CPU 利用率 = 忙碌 tick / 总 tick；
- 吞吐量 = 完成进程数 / 总 tick。

`getMetrics()` 只统计已终止的进程，便于单步调试时查看当前进度。

## 复杂度

- 选择进程：O(n) 线性扫描进程表（n 为登记进程数）；
- 抢占判断：O(n)；
- 老化：O(n)，每 `agingInterval` 个 tick 执行一次。

进程数很大时可换用以优先级为键的堆，但课程规模下线性扫描更易读、更易验证。

## 与其它模块的关系

- `CLOCK`：唯一的时间来源；
- `CPU`：执行一个 tick，由调度器负责装载与卸载；
- `PCB`：保存状态、优先级、剩余时间与统计；调度器只引用不复制；
- 优先级如何分配属于策略问题，本模块只按数值比较，不做业务解释。

## 测试

`tests/PSTest.cpp` 覆盖抢占顺序与统计指标、非抢占模式、同优先级按到达时间/PID
决出先后、老化提升开始时刻、空系统与非法参数：

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```
