//
// Created by YuHao Shi on 2026/11/6.
//

#ifndef OSDESIGN_PROCESSCONTROLBLOCK_H
#define OSDESIGN_PROCESSCONTROLBLOCK_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ProcessControlBlock {
public:
    enum class ProcessState { New, Ready, Running, Blocked, Suspended, Terminated };

    struct CPUContext {
        std::uint64_t programCounter = 0;
        std::array<std::uint64_t, 16> registers{};
        std::uint64_t stackPointer = 0;
        bool operator==(const CPUContext&) const = default;
    };

    // 时间单位为模拟时钟 tick；PID 的全局唯一性由进程管理器保证。
    ProcessControlBlock(int pid, std::string name, int burstTime, int priority = 0, int ppid = -1,
                        int arrivalTime = 0);

    int getPid() const { return pid_; }
    int getParentPid() const { return ppid_; }
    const std::string& getName() const { return name_; }
    ProcessState getState() const { return state_; }
    int getPriority() const { return priority_; }
    void setPriority(int priority) { priority_ = priority; }
    std::optional<int> getExitCode() const { return exitCode_; }

    // 非法状态转换抛出 std::logic_error；非法参数抛出 std::invalid_argument。
    void setReady();
    void setRunning(int currentTime);
    void setBlocked(const std::string& reason);
    void wakeup();
    void suspend();
    void resume();
    void terminate(int currentTime, int exitCode = 0);
    bool isReady() const { return state_ == ProcessState::Ready; }
    bool isRunning() const { return state_ == ProcessState::Running; }
    bool isBlocked() const { return state_ == ProcessState::Blocked; }
    bool isSuspended() const { return state_ == ProcessState::Suspended; }
    bool isTerminated() const { return state_ == ProcessState::Terminated; }
    const std::string& getBlockedReason() const { return blockedReason_; }
    std::optional<ProcessState> getSuspendedState() const { return suspendedState_; }

    int getArrivalTime() const { return arrivalTime_; }
    int getBurstTime() const { return burstTime_; }
    int getRemainingTime() const { return remainingTime_; }
    int getCpuTime() const { return cpuTime_; }
    int getWaitTime() const { return waitTime_; }
    std::optional<int> getStartTime() const { return startTime_; }
    std::optional<int> getFinishTime() const { return finishTime_; }
    std::optional<int> getTurnaroundTime() const;
    int getTimeSlice() const { return timeSlice_; }
    int getRemainingTimeSlice() const { return remainingTimeSlice_; }
    void resetTimeSlice(int slice);
    // 仅 Running 状态可推进执行；同时扣减运行预算和已配置的时间片。
    void updateRuntime(int ticks);
    bool consumeTimeSlice(); // 执行一个 tick，返回时间片是否耗尽。
    void updateWaitTime(int ticks); // 调度器为 Ready 状态累计等待时间。
    bool isFinished() const { return remainingTime_ == 0; }

    void saveContext(const CPUContext& context) { context_ = context; }
    CPUContext restoreContext() const { return context_; }

    // 只记录内存映射，不实际分配/释放内存；由内存管理器调用。
    void setMemoryInfo(std::uint64_t base, std::size_t size);
    void clearMemoryInfo();
    std::uint64_t getMemoryBase() const { return memoryBase_; }
    std::size_t getMemorySize() const { return memorySize_; }
    void setPageTable(std::vector<std::size_t> pageTable);
    const std::vector<std::size_t>& getPageTable() const { return pageTable_; }
    void setSwapped(bool swapped) { isSwapped_ = swapped; }
    bool isSwapped() const { return isSwapped_; }

    // 资源 ID 表示独立资源实例；重复登记会被忽略。
    void addResource(int resourceId);
    bool releaseResource(int resourceId);
    const std::vector<int>& getHeldResources() const { return heldResources_; }
    void setWaitingResource(int resourceId);
    void clearWaitingResource() { waitingResource_.reset(); }
    std::optional<int> getWaitingResource() const { return waitingResource_; }
    void addChild(int pid);
    bool removeChild(int pid);
    const std::vector<int>& getChildren() const { return children_; }
    void addOpenFile(const std::string& filename);
    bool removeOpenFile(const std::string& filename);
    const std::vector<std::string>& getOpenFiles() const { return openFiles_; }

    static std::string_view stateToString(ProcessState state);
    std::string toString() const;
    void print(std::ostream& output) const;
    void print() const;

private:
    void validateCurrentTime(int currentTime) const;

    int pid_;
    int ppid_;
    std::string name_;
    ProcessState state_ = ProcessState::New;
    int priority_;
    std::optional<int> exitCode_;
    std::optional<ProcessState> suspendedState_;
    std::string blockedReason_;

    int arrivalTime_;
    int burstTime_;
    int remainingTime_;
    int cpuTime_ = 0;
    int waitTime_ = 0;
    int timeSlice_ = 0;
    int remainingTimeSlice_ = 0;
    std::optional<int> startTime_;
    std::optional<int> finishTime_;
    // 最近一次调度/执行的时刻，防止模拟时钟倒退。
    int lastTime_;
    CPUContext context_;

    std::uint64_t memoryBase_ = 0;
    std::size_t memorySize_ = 0;
    std::vector<std::size_t> pageTable_;
    bool isSwapped_ = false;
    std::vector<int> heldResources_;
    std::optional<int> waitingResource_;
    std::vector<int> children_;
    std::vector<std::string> openFiles_;
};

#endif //OSDESIGN_PROCESSCONTROLBLOCK_H
