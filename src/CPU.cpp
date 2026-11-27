#include "../include/CPU.h"

CPU::State CPU::execute(Instruction instruction) {
    if (pcb == nullptr || pcb->state != PCB::RUNNING) {
        return ERROR;
    }

    switch (instruction.type) {
        case Instruction::COMPUTE:
            if (instruction.argument <= 0) {
                return ERROR;
            }
            if (pcb->instructionRemainingTicks == 0) {
                pcb->instructionRemainingTicks = instruction.argument;
            }
            --pcb->instructionRemainingTicks;
            ++pcb->CPUTime;
            if (pcb->instructionRemainingTicks == 0) {
                ++pcb->programCounter;
            }
            return CONTINUE;

        case Instruction::IO:
        case Instruction::READ:
            if (instruction.argument <= 0 || pcb->instructionRemainingTicks != 0) {
                return ERROR;
            }
            pcb->ioRemainingTicks = instruction.argument;
            // 请求只提交一次；阻塞期间禁止执行，唤醒后从下一条指令继续。
            ++pcb->programCounter;
            pcb->state = PCB::BLOCK;
            return IO;

        case Instruction::Yield:
            if (instruction.argument != 0 || pcb->instructionRemainingTicks != 0) {
                return ERROR;
            }
            ++pcb->programCounter;
            pcb->state = PCB::READY;
            return YIELDED;

        case Instruction::Exit:
            if (pcb->instructionRemainingTicks != 0) {
                return ERROR;
            }
            pcb->exitCode = instruction.argument;
            ++pcb->programCounter;
            pcb->state = PCB::END;
            return END;

        default: return ERROR;
    }
}
