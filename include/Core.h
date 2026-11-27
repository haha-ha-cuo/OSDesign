#ifndef OSDESIGN_OS_H
#define OSDESIGN_OS_H
#include "Dram.h"
#include "PCB.h"
#include "Process.h"
#include <string>

class Core {
public:
    enum State {
        SUCCESS,
        FAILURE,
    };
    // 成功时输出主存中的 CREATING 槽位；表满时输出 nullptr，调用方不得 delete。
    State createProcess(PCB*& pcb);
    State createChildProcess(PCB* pcb);
    /**
     * 逐行加载指令，跳过空白行；成功时替换进程原有程序。
     * 空指针、空程序、文件读入或解析失败时返回 FAILURE，并保留原有程序。
     */
    State loadfile(Process* process, const std::string& filename);
    Dram dram;
    int pid_ = 1;
    int allocatePid() { return pid_++; }
};

#endif //OSDESIGN_OS_H
