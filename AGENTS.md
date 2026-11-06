# AGENTS.md

本文件为在本仓库中工作的 AI/自动化代理提供约定。除非用户另有说明，请遵守。

## 项目概览

- C++20 操作系统模拟系统，使用 CMake 构建。
- 头文件位于 `include/`，实现位于 `src/`，测试位于 `tests/`，设计文档位于 `docs/`。

## 构建与测试

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 代码格式化（重要）

本项目使用 clang-format 统一代码风格，规则定义在根目录的 `.clang-format` 中
（基于 LLVM 风格，4 空格缩进，行宽 100）。

**每次修改或新增 C++ 文件后、结束任务前，必须执行：**

```bash
cmake --build build --target format
```

**并确认格式检查通过：**

```bash
cmake --build build --target format-check
```

要求：

- 格式化统一通过 CMake 目标完成，不要手动调整格式，也不要修改 `.clang-format` 以迁就某次改动。
- `format` 会格式化 `include/`、`src/`、`tests/` 下的全部源码。若因此产生与本次任务无关的大面积格式变化，不要把这些无关改动混入本次提交。
- 如果提示找不到 clang-format，请先安装（如 macOS 执行 `brew install clang-format`），
  或通过 `cmake -S . -B build -DCLANG_FORMAT_EXECUTABLE=<path>` 指定路径；无法解决时在最终回复中说明。
- 格式化完成后重新编译并运行测试，确保格式化未破坏代码。
- 不要用其他格式化工具覆盖 clang-format 的结果。

## 其他约定

- 遵循现有代码风格与命名习惯。
- 新增功能应补充对应测试，并保证 `ctest` 全部通过。
- 不提交 `build/`、`cmake-build-*/`、`.idea/` 等构建产物或 IDE 配置。
- 提交信息应简洁描述改动内容，例如 `feat: 实现时间片轮转调度`。
