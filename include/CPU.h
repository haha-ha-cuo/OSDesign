#ifndef OSDESIGN_CPU_H
#define OSDESIGN_CPU_H
#include "Instruction.h"
#include "PCB.h"

class CPU {
public:
    int id;
    PCB* pcb = nullptr;
    enum State { ERROR, IO, END, CONTINUE, YIELDED };
    /**
     * 调用方传入 programCounter 对应的指令，且 PCB 必须处于 RUNNING 状态。
     * COMPUTE 每次消耗一个 CPU tick；完成或提交请求后推进 programCounter。
     * IO/READ 返回 IO，由内核提交设备请求并在完成后唤醒；全局时钟由内核推进。
     */
    State execute(Instruction instruction);
    void contextShifter() { pcb = pcb->next; };
};

#endif //OSDESIGN_CPU_H
