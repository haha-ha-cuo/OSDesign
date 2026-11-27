#include "CPU.h"

#include <iostream>
#include <stdexcept>

int main() {
    try {
        CPU cpu;
        if (cpu.execute({Instruction::COMPUTE, 1}) != CPU::ERROR) {
            throw std::runtime_error("空闲 CPU 不应执行指令");
        }

        PCB first;
        PCB second;
        cpu.pcb = &first;
        if (cpu.execute({Instruction::COMPUTE, 1}) != CPU::ERROR || first.CPUTime != 0) {
            throw std::runtime_error("非运行状态不应执行指令");
        }
        first.state = PCB::RUNNING;
        if (cpu.execute({Instruction::COMPUTE, 3}) != CPU::CONTINUE ||
            first.instructionRemainingTicks != 2 || first.CPUTime != 1 ||
            first.programCounter != 0) {
            throw std::runtime_error("计算指令未按一个 tick 执行");
        }

        // 模拟抢占后运行另一进程，再恢复原进程。
        first.state = PCB::READY;
        second.state = PCB::RUNNING;
        cpu.pcb = &second;
        if (cpu.execute({Instruction::COMPUTE, 1}) != CPU::CONTINUE || second.programCounter != 1 ||
            second.CPUTime != 1) {
            throw std::runtime_error("第二个进程未独立执行");
        }
        cpu.pcb = &first;
        first.state = PCB::RUNNING;
        cpu.execute({Instruction::COMPUTE, 3});
        if (first.instructionRemainingTicks != 1 || first.programCounter != 0) {
            throw std::runtime_error("恢复进程后丢失计算进度");
        }
        cpu.execute({Instruction::COMPUTE, 3});
        if (first.instructionRemainingTicks != 0 || first.programCounter != 1 ||
            first.CPUTime != 3) {
            throw std::runtime_error("计算指令完成后未正确推进");
        }

        // 即使已关联设备，发起异步请求仍需等待完成。
        IO device;
        first.io = &device;
        if (cpu.execute({Instruction::IO, 4}) != CPU::IO || first.state != PCB::BLOCK ||
            first.ioRemainingTicks != 4 || first.programCounter != 2 || first.CPUTime != 3) {
            throw std::runtime_error("I/O 请求未正确阻塞或错误消耗 CPU 时间");
        }
        if (cpu.execute({Instruction::IO, 4}) != CPU::ERROR || first.programCounter != 2) {
            throw std::runtime_error("阻塞期间重复提交请求");
        }

        // 模拟设备完成、内核唤醒并重新调度。
        first.ioRemainingTicks = 0;
        first.state = PCB::RUNNING;
        if (cpu.execute({Instruction::Yield, 0}) != CPU::YIELDED || first.state != PCB::READY ||
            first.programCounter != 3) {
            throw std::runtime_error("YIELD 未主动让出 CPU");
        }
        first.state = PCB::RUNNING;
        if (cpu.execute({Instruction::READ, 2}) != CPU::IO || first.state != PCB::BLOCK ||
            first.ioRemainingTicks != 2 || first.programCounter != 4) {
            throw std::runtime_error("READ 未生成异步请求");
        }
        first.ioRemainingTicks = 0;
        first.state = PCB::RUNNING;
        if (cpu.execute({Instruction::Exit, -1}) != CPU::END || first.state != PCB::END ||
            first.exitCode != -1 || first.programCounter != 5 || first.CPUTime != 3) {
            throw std::runtime_error("退出状态或退出码错误");
        }
        if (cpu.execute({Instruction::COMPUTE, 1}) != CPU::ERROR) {
            throw std::runtime_error("已结束的进程不应继续执行");
        }

        PCB invalid;
        invalid.state = PCB::RUNNING;
        cpu.pcb = &invalid;
        for (const Instruction instruction :
             {Instruction{Instruction::COMPUTE, 0}, Instruction{Instruction::IO, -1},
              Instruction{Instruction::READ, 0}, Instruction{Instruction::Yield, 1}}) {
            if (cpu.execute(instruction) != CPU::ERROR || invalid.programCounter != 0 ||
                invalid.CPUTime != 0 || invalid.state != PCB::RUNNING) {
                throw std::runtime_error("非法参数改变了执行状态");
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
