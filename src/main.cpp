#include "../include/MLFQ.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
struct DemoProcess {
    PCB pcb;
    int level = 1;
};
} // namespace

int main() {
    try {
        CLOCK clock;
        // 进程表固定大小、地址稳定，且比 CPU 和调度器活得更久。
        std::array<DemoProcess, 3> processes{{
            {PCB(1, "short", 3)},
            {PCB(2, "medium", 10)},
            {PCB(3, "long", 25)},
        }};
        CPU cpu(0, clock);
        MLFQ scheduler(cpu, 4);

        // 1. 所有进程在时刻 0 到达，New -> Ready，进入 Q1。
        for (auto& process : processes) {
            scheduler.receivePCB(process.pcb);
            std::cout << "[t=0] 创建 PID=" << process.pcb.getPid()
                      << "，工作量=" << process.pcb.getBurstTime() << "，进入 Q1\n";
        }

        std::size_t finished = 0;
        while (finished < processes.size()) {
            // 2. CPU 空闲时派发。dispatch() 自动从 PCB 恢复上下文。
            const bool dispatched = scheduler.ProcessShifter();
            auto running = std::find_if(processes.begin(), processes.end(), [&](const auto& p) {
                return &p.pcb == cpu.getCurrentProcess();
            });
            if (running == processes.end()) {
                throw std::logic_error("Unfinished demo has no runnable process");
            }
            PCB& pcb = running->pcb;
            if (dispatched) {
                std::cout << "[t=" << clock.now() << "] 派发 PID=" << pcb.getPid() << "，来自 Q"
                          << running->level << "，恢复上下文 R0=" << cpu.getContext().registers[0]
                          << '\n';
            }

            // 3. 执行一拍。CPU 已更新运行时间，不能再调用 updateRuntime()。
            const auto result = cpu.tick();
            // 仅为展示上下文保存/恢复：用 R0 记录完成的工作单位，不模拟真实指令。
            auto context = cpu.getContext();
            ++context.registers[0];
            cpu.setContext(context);

            for (auto& process : processes) {
                if (process.pcb.isReady()) {
                    process.pcb.updateWaitTime(1);
                }
            }
            clock.advance(); // 整个系统每拍只推进一次，随后才能卸载 CPU。
            std::cout << "[t=" << clock.now() << "] PID=" << pcb.getPid()
                      << " 已运行=" << pcb.getCpuTime() << "，剩余=" << pcb.getRemainingTime()
                      << '\n';

            // 4. 节拍边界处理结果。release() 自动保存上下文到原 PCB。
            if (result == CPU::TickResult::Finished) {
                pcb.terminate(clock.now()); // 完成时必须先终止，再卸载。
                cpu.release();
                ++finished;
                std::cout << "  完成并卸载 PID=" << pcb.getPid() << '\n';
            } else if (result == CPU::TickResult::TimeSliceExpired) {
                cpu.release(); // Running -> Ready。
                running->level = std::min(running->level + 1, MLFQ::kLevels);
                scheduler.receivePCB(pcb, running->level);
                std::cout << "  时间片耗尽，保存 R0=" << pcb.restoreContext().registers[0]
                          << "，PID=" << pcb.getPid() << " 进入 Q" << running->level << " 队尾\n";
            }
        }

        // 5. 打印统计，并验证工作量、时间与上下文均未因切换而丢失。
        std::cout << "\nPID\t名称\tCPU时间\t等待时间\t完成时刻\t周转时间\t保存的R0\n";
        int totalCpuTime = 0;
        for (const auto& process : processes) {
            const PCB& pcb = process.pcb;
            const auto savedWork = pcb.restoreContext().registers[0];
            if (!pcb.isTerminated() || pcb.getCpuTime() != pcb.getBurstTime() ||
                savedWork != static_cast<std::uint64_t>(pcb.getCpuTime()) ||
                pcb.getTurnaroundTime() != pcb.getCpuTime() + pcb.getWaitTime()) {
                throw std::logic_error("Demo accounting or context verification failed");
            }
            totalCpuTime += pcb.getCpuTime();
            std::cout << pcb.getPid() << '\t' << pcb.getName() << '\t' << pcb.getCpuTime() << '\t'
                      << pcb.getWaitTime() << '\t' << *pcb.getFinishTime() << '\t'
                      << *pcb.getTurnaroundTime() << '\t' << savedWork << '\n';
        }
        if (!cpu.isIdle() || totalCpuTime != clock.now() || scheduler.ProcessShifter()) {
            throw std::logic_error("Demo did not finish with an empty CPU and ready queues");
        }
        std::cout << "全部进程完成，总模拟时间=" << clock.now() << " tick\n";
    } catch (const std::exception& error) {
        std::cerr << "模拟失败: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
