#include "../include/Simulation.h"
#include "../include/TUI.h"

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void runDemo() {
    Simulation simulation;
    simulation.addProcess("short", 3);
    simulation.addProcess("medium", 10);
    simulation.addProcess("long", 25);
    const auto& processes = simulation.processes();
    for (const auto& process : processes) {
        std::cout << "[t=0] 创建 PID=" << process.pcb.getPid()
                  << "，工作量=" << process.pcb.getBurstTime() << "，进入 Q1\n";
    }

    std::size_t finished = 0;
    while (finished < processes.size()) {
        const int startTime = simulation.time();
        const auto tick = simulation.step();
        if (!tick.process) {
            throw std::logic_error("Unfinished demo has no runnable process");
        }
        const PCB& pcb = tick.process->pcb;
        if (tick.dispatched) {
            std::cout << "[t=" << startTime << "] 派发 PID=" << pcb.getPid() << "，来自 Q"
                      << tick.queueLevel << "，恢复上下文 R0=" << tick.previousR0 << '\n';
        }

        std::cout << "[t=" << simulation.time() << "] PID=" << pcb.getPid()
                  << " 已运行=" << pcb.getCpuTime() << "，剩余=" << pcb.getRemainingTime() << '\n';

        if (tick.result == CPU::TickResult::Finished) {
            ++finished;
            std::cout << "  完成并卸载 PID=" << pcb.getPid() << '\n';
        } else if (tick.result == CPU::TickResult::TimeSliceExpired) {
            std::cout << "  时间片耗尽，保存 R0=" << pcb.restoreContext().registers[0]
                      << "，PID=" << pcb.getPid() << " 进入 Q" << tick.process->queueLevel
                      << " 队尾\n";
        }
    }

    // 打印统计，并验证工作量、时间与上下文均未因切换而丢失。
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
    if (!simulation.cpu().isIdle() || totalCpuTime != simulation.time()) {
        throw std::logic_error("Demo did not finish with an empty CPU and ready queues");
    }
    std::cout << "全部进程完成，总模拟时间=" << simulation.time() << " tick\n";
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) {
            runDemo();
        } else if (argc == 2 && std::string_view(argv[1]) == "--tui") {
            TUI{}.run();
        } else {
            std::cerr << "用法: OSDesign [--tui]\n";
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "模拟失败: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
