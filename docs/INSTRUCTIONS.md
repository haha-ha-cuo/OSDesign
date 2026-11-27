# 指令文件与进程启动演示

## 运行

在仓库根目录执行：

```bash
cmake -S . -B build
cmake --build build -j
./build/OSDesign examples/startup.txt
```

`examples/startup.txt` 是实际加载的指令文件：

```text
COMPUTE 3
```

它要求执行 3 个 CPU tick。本次演示只执行其中一个 tick，
观察进程进入 RUNNING 后的执行进度。

## 演示流程

入口位于 `main.cpp`。

1. `Core::createProcess()` 在主存固定的 16 个 PCB 槽位中寻找 BLANK 槽位，
   分配 PID，将状态改为 CREATING。
2. 建立 `Process::pcb` 与 `PCB::process` 的双向关联。
3. `Core::loadfile()` 读取指令文件，解析并保存到 `Process::programs`。
4. 加载成功后，将 PCB 改为 READY。
5. 将 PCB 交给 `CPU::pcb`，状态改为 RUNNING。
6. 根据 PCB 的 `programCounter` 找到指令，调用 `CPU::execute()` 执行一个 tick，
   由演示入口将全局时钟推进一个 tick。

预期输出：

```text
[t=0 PID=1] BLANK → CREATING：分配主存 PCB，绑定 Process
[t=0 PID=1] CREATING → READY：已载入 1 条指令
[t=0 PID=1] READY → RUNNING：调度到 CPU
[t=1 PID=1] COMPUTE 执行 1 tick，剩余 2 tick，PC=0
SUMMARY state=RUNNING CPU=1 elapsed=1 remaining=2
```

`PC=0` 表示第一条指令尚未完成，再次执行时应继续它的剩余 2 tick。
演示在 RUNNING 状态下停止展示；宿主程序随后返回，不模拟 EXIT 或 PCB 回收，
也不进行时间片切换。

## 当前指令格式

每行一条，大写关键字，允许空白行，不支持行内注释。

| 指令 | 参数 | 当前 CPU 行为 |
| --- | --- | --- |
| `COMPUTE 3` | 正整数时钟数 | 每次执行消耗一个 CPU tick，保存剩余量 |
| `IO 3` | 正整数服务时钟数 | 记录请求时长，进入 BLOCK，返回 IO 事件 |
| `READ 2` | 正整数服务时钟数 | 与 IO 相同，模拟读取请求 |
| `YIELD` | 无 | 进入 READY，返回主动让出事件 |
| `EXIT 0` | 整数退出码 | 保存退出码，进入 END |

此启动演示要求第一条指令为 COMPUTE。其他指令的解析与 CPU 分支已存在，
但设备服务、阻塞唤醒及进程回收不在本次演示中实现。

文件路径相对于运行时工作目录。文件不存在、为空或格式错误时加载失败；
演示会报错返回，失败后的 PCB 回收留待后续实现。
