# 操作系统模拟系统（OSDesign）

> 操作系统课程设计项目：基于 C++20 实现一个允许多进程运行和管理的模拟操作系统。

## 项目简介

本项目围绕操作系统课程设计题目「操作系统模拟系统」展开，目标是在用户态模拟操作系统的核心机制，包括进程控制、进程调度、状态切换、互斥与同步、进程通信、死锁处理、内存分配、外存访问以及线程机制等，并通过命令行界面进行演示与测试。

通过完整实现各功能模块并撰写设计报告，加深对操作系统基本原理与实现方法的理解。

## 功能规划

| 模块 | 主要内容 |
| --- | --- |
| 进程管理 | PCB 设计；进程创建与撤销；就绪、运行、阻塞等状态切换；阻塞/唤醒、挂起/激活原语 |
| 进程调度 | 实现至少两种经典调度算法，如 FCFS、SJF、时间片轮转（RR）、优先级调度等 |
| 互斥与同步 | 互斥锁、信号量、PV 操作；生产者-消费者、读者-写者等经典同步问题 |
| 进程通信 | 共享内存、消息队列、管道等进程间通信方式的模拟 |
| 死锁处理 | 死锁预防、银行家算法（避免）、死锁检测与恢复 |
| 内存管理 | 内存分配与回收；首次适应、最佳适应等分配算法；分页/分段；换入换出 |
| 外存访问 | 模拟磁盘与文件系统；文件创建、读写、删除等操作 |
| 线程机制 | 用户级线程的创建、调度与撤销 |
| 交互界面 | 命令行菜单与命令解析，支持查看进程、内存、调度等系统状态 |

> 当前支持 PCB、模拟 CPU 和时钟、MLFQ 调度、优先级调度（PS）与交互式 TUI。详见 [PCB](docs/PCB.md)、[MLFQ](docs/MLFQ.md) 和 [PS](docs/PS.md) 文档。

## 技术栈

- **语言**：C++20
- **构建工具**：CMake 3.20+
- **代码格式化**：clang-format
- **编译器**：GCC 11+、Clang 14+ 或 MSVC 19.30+
- **第三方依赖**：FTXUI v7.0.0，由 CMake 在首次配置时通过 Git 下载

## 目录结构

```text
OSDesign/
├── CMakeLists.txt     # CMake 构建配置
├── main.cpp           # 程序入口
├── README.md          # 项目说明
├── include/           # 头文件（规划）
├── src/               # 功能模块实现（规划）
└── docs/              # 设计文档与课程报告（规划）
```

## 构建与运行

### 环境准备

- 安装 CMake 3.20 及以上版本，并确保 `cmake` 已加入 `PATH`。
- 安装 Git，首次配置时 CMake 会下载 FTXUI。
- 安装支持 C++20 的编译器：
  - **Windows**：Visual Studio 2022（安装「使用 C++ 的桌面开发」工作负载）
  - **macOS**：Xcode Command Line Tools（`xcode-select --install`）
  - **Linux**：GCC 11+ 或 Clang 14+（如 `build-essential`）

### 使用 CLion（推荐）

1. 打开 CLion，选择 **File → Open**，打开项目根目录 `OSDesign`。
2. 等待 CMake 自动加载完成；若未自动加载，右键 `CMakeLists.txt` 选择 **Reload CMake Project**。
3. 在右上角运行配置中选择 `OSDesign`。
4. 点击 **Run**（⌃R / Shift+F10）运行程序。

### 使用命令行

在项目根目录执行：

**macOS / Linux**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/OSDesign --tui
```

**Windows（PowerShell）**

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\OSDesign.exe --tui
```

> 说明：使用 Makefile、Ninja 等单配置生成器时，可执行文件位于 `build/OSDesign`；使用 Visual Studio、Xcode 等多配置生成器时，可执行文件位于 `build/Release/`（或 `build/Debug/`）下。

### TUI 操作

界面启动时，模拟时钟暂停在第 0 拍。在底部输入命令并按 Enter。用 Tab 在命令行和进程列表之间切换，用方向键浏览列表。

| 命令 | 行为 |
| --- | --- |
| `add editor 10` | 创建一个需要 10 个 tick 的进程，进入 Q1。进程名使用单个词，tick 必须是正整数。 |
| `step` | 暂停自动时钟并手动推进一拍。 |
| `run` | 启动自动时钟，每 500 毫秒推进一拍。 |
| `pause` | 暂停自动时钟。 |
| `quit` | 退出界面。 |

时钟面板显示当前 tick 和最近一拍的事件。CPU 面板显示当前 PID 和剩余时间片。进程列表保留已完成的进程，便于对照运行结果。

TUI 使用 MLFQ，每个队列的时间片均为 4 tick。时间片耗尽后降一级，最低保持 Q5；
高级队列有新进程时，在下一拍开始前抢占低级队列中的运行进程，被抢占进程回到原级队尾。

直接运行 `OSDesign` 可查看批处理调度演示。

### 常见问题

| 问题 | 解决方法 |
| --- | --- |
| 找不到 `cmake` 命令 | 安装 CMake 并配置环境变量，或直接使用 CLion 内置的 CMake |
| CMake 版本过低 | 升级到 3.20 及以上版本 |
| 编译器不支持 C++20 | 升级 GCC/Clang/MSVC，或在 IDE 中切换工具链 |
| macOS 提示缺少编译工具 | 执行 `xcode-select --install` 安装命令行工具 |

### 代码格式化

项目使用 [clang-format](https://clang.llvm.org/docs/ClangFormat.html) 统一 C++ 代码风格，
规则定义在根目录的 `.clang-format` 中。格式化通过 CMake 目标执行：

```bash
# 格式化 include/、src/、tests/ 下的全部 C++ 源码
cmake --build build --target format

# 只检查格式是否符合规范，不修改文件
cmake --build build --target format-check
```

CMake 会优先使用 `-DCLANG_FORMAT_EXECUTABLE=<path>` 指定的版本，未指定时在 `PATH` 中查找；
macOS 上若找不到，会自动回退到 Xcode 自带的 clang-format。也可以手动安装：

- **macOS**：`brew install clang-format`
- **Ubuntu / Debian**：`sudo apt install clang-format`
- **Windows**：安装 LLVM，并确保 `clang-format.exe` 位于 `PATH` 中

## 组队与分工

- 每 1~4 人一组，自由组队。
- 每组提交一份课程设计报告，并在报告中明确每位成员的分工与职责。
- 建议按「进程与调度」「同步与通信」「内存与外存」「界面与测试」等模块分工，最终统一集成。

## 提交要求

- **截止时间**：第 18 周周五晚上 22:00 前。
- **提交内容**：电子版课程设计报告 + 实现代码。
- 报告应包含需求分析、系统设计、关键数据结构与算法、功能实现与测试、成员分工、总结等内容。

## 开发约定

- 提交前确保项目可以正常构建和运行。
- 提交信息应简洁描述改动内容，例如 `feat: 实现时间片轮转调度`。
- 不提交 IDE 配置和构建产物（已在 `.gitignore` 中忽略 `.idea/`、`cmake-build-*/` 和 `build/`）。
