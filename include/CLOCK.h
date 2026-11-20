//
// Created by YuHao Shi on 2026/11/13.
//

#ifndef OSDESIGN_CLOCK_H
#define OSDESIGN_CLOCK_H
// 全局模拟时钟：由系统持有一个实例，统一使用 tick 作为时间单位。
// 不读取真实时间、不创建线程，也不直接操作 PCB 或调用调度器。
class CLOCK {
public:
    // 与 PCB 的时间类型一致；initialTime 必须非负。
    explicit CLOCK(int initialTime = 0);

    [[nodiscard]] int now() const noexcept { return currentTime_; }

    // 推进正数个 tick，返回推进后的时刻。
    // 非正数抛出 std::invalid_argument；溢出抛出 std::overflow_error。
    int advance(int ticks = 1);

    // 仅用于开始一轮新模拟；调用方必须同时重建/重置进程等系统状态。
    void reset(int initialTime = 0);

private:
    int currentTime_;
};

#endif //OSDESIGN_CLOCK_H
