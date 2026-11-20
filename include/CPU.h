//
// Created by YuHao Shi on 2026/11/13.
//

#ifndef OSDESIGN_CPU_H
#define OSDESIGN_CPU_H
#include "CLOCK.h"
#include "PCB.h"

// 单个模拟 CPU 核心；不选择进程、不推进全局时钟。
class CPU {
public:
    enum class Status { FREE, OCCUPIED };
    enum class TickResult { Idle, Running, TimeSliceExpired, Finished };

    CPU(int id, const CLOCK& clock);
    CPU(const CPU&) = delete;
    CPU& operator=(const CPU&) = delete;

    int getId() const noexcept { return id_; }
    Status getStatus() const noexcept { return isIdle() ? Status::FREE : Status::OCCUPIED; }
    bool isIdle() const noexcept { return currentProcess_ == nullptr; }
    const PCB* getCurrentProcess() const noexcept { return currentProcess_; }

    // PCB 必须处于 Ready 状态且有剩余工作；时刻由共享时钟提供。
    // CPU 不拥有 PCB，调用方必须保证该对象在 release() 前存活且地址不变。
    void dispatch(PCB& process);

    // 执行区间 [clock.now(), clock.now() + 1)，但不推进全局时钟。
    // 返回结果供调度器处理，不自动终止进程、抢占或切换队列。
    // 同一时刻不可重复执行；运行期间也不能跳过时钟节拍。
    TickResult tick();

    // 保存上下文并卸载，返回原 PCB；空闲时返回 nullptr。
    // Running -> Ready（抢占）；已阻塞/终止等状态保持不变。
    // 已完成的运行进程须先 terminate()；执行完 tick 须先推进全局时钟。
    PCB* release();

    const PCB::CPUContext& getContext() const noexcept { return context_; }
    void setContext(const PCB::CPUContext& context);

private:
    int id_;
    const CLOCK& clock_;
    PCB* currentProcess_ = nullptr;
    PCB::CPUContext context_{};
    int nextTickTime_;
};

#endif //OSDESIGN_CPU_H
