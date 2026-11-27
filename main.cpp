#include "CLOCK.h"
#include "CPU.h"
#include "Core.h"
#include "Process.h"

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "用法：OSDesign <指令文件路径>\n";
        return 1;
    }

    Core core;
    CPU cpu;
    Process process;
    PCB* pcb = nullptr;
    CLOCK::t = 0;

    if (core.createProcess(pcb) != Core::SUCCESS) {
        std::cerr << "创建失败：没有空白 PCB\n";
        return 1;
    }
    pcb->process = &process;
    process.pcb = pcb;
    pcb->name = "process-demo";
    std::cout << "[t=0 PID=" << pcb->pid << "] BLANK → CREATING：分配主存 PCB，绑定 Process\n";

    if (core.loadfile(&process, argv[1]) != Core::SUCCESS) {
        std::cerr << "加载失败：文件不可读、为空或指令格式错误\n";
        return 1;
    }
    if (process.programs.front().type != Instruction::COMPUTE) {
        std::cerr << "此演示要求第一条指令为 COMPUTE\n";
        return 1;
    }
    pcb->state = PCB::READY;
    std::cout << "[t=0 PID=" << pcb->pid << "] CREATING → READY：已载入 " << process.programs.size()
              << " 条指令\n";

    pcb->state = PCB::RUNNING;
    cpu.pcb = pcb;
    std::cout << "[t=0 PID=" << pcb->pid << "] READY → RUNNING：调度到 CPU\n";

    // 演示只执行一个计算 tick，保留 RUNNING 状态及剩余进度供观察。
    const auto instruction = pcb->process->programs[pcb->programCounter];
    if (cpu.execute(instruction) != CPU::CONTINUE) {
        std::cerr << "执行失败：CPU 拒绝计算指令\n";
        return 1;
    }
    CLOCK::tick(1);
    std::cout << "[t=" << CLOCK::t << " PID=" << pcb->pid << "] COMPUTE 执行 1 tick，剩余 "
              << pcb->instructionRemainingTicks << " tick，PC=" << pcb->programCounter << '\n';
    std::cout << "SUMMARY state=RUNNING CPU=" << pcb->CPUTime << " elapsed=" << CLOCK::t
              << " remaining=" << pcb->instructionRemainingTicks << '\n';
    return 0;
}
