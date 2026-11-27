//
// Created by YuHao Shi on 2026/11/20.
//

#include "../include/TUI.h"
#include "../include/Simulation.h"

#include <ftxui/ftxui.hpp>

#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace ftxui;
using namespace std::chrono_literals;

constexpr auto kTickInterval = 500ms;

Element clockPanel(int time, bool running, const std::string& lastSignal) {
    return window(text("模拟时钟") | bold,
                  vbox({
                      hbox({text("Tick  "), text(std::to_string(time)) | bold | color(Color::Cyan),
                            filler(), text(running ? "自动 · 500 ms/拍" : "暂停")}),
                      text(lastSignal),
                  }));
}

Element cpuPanel(const CPU& cpu) {
    const PCB* process = cpu.getCurrentProcess();
    const auto state =
        text(process ? "运行中" : "空闲") | bold | color(process ? Color::Green : Color::Yellow);
    return window(text("CPU " + std::to_string(cpu.getId())) | bold,
                  vbox({
                      hbox({state, filler(),
                            text(process ? "PID " + std::to_string(process->getPid()) : "PID —")}),
                      text(process ? "时间片 " + std::to_string(process->getRemainingTimeSlice()) +
                                         "/" + std::to_string(process->getTimeSlice()) + " tick"
                                   : "时间片 —"),
                  }));
}

Element processRow(std::string pid, std::string name, Element state, std::string queue,
                   std::string elapsed, std::string remaining) {
    return hbox({
        text(std::move(pid)) | size(WIDTH, EQUAL, 7),
        text(std::move(name)) | size(WIDTH, GREATER_THAN, 12) | flex,
        std::move(state) | size(WIDTH, EQUAL, 8),
        text(std::move(queue)) | size(WIDTH, EQUAL, 7),
        text(std::move(elapsed)) | size(WIDTH, EQUAL, 11),
        text(std::move(remaining)) | size(WIDTH, EQUAL, 10),
    });
}

Element processState(const PCB& pcb) {
    if (pcb.isRunning()) {
        return text("运行中") | color(Color::Green);
    }
    if (pcb.isReady()) {
        return text("就绪") | color(Color::Yellow);
    }
    if (pcb.isTerminated()) {
        return text("已完成");
    }
    return text(std::string(PCB::stateToString(pcb.getState())));
}

std::string describeTick(const Simulation::Tick& tick) {
    if (!tick.process) {
        return "CPU 空闲";
    }
    const auto pid = "PID " + std::to_string(tick.process->pcb.getPid());
    if (tick.result == CPU::TickResult::Finished) {
        return pid + " 已完成";
    }
    if (tick.result == CPU::TickResult::TimeSliceExpired) {
        return pid + " 时间片耗尽 → Q" + std::to_string(tick.process->queueLevel);
    }
    return pid + (tick.dispatched ? " 已派发并运行" : " 运行一拍");
}
} // namespace

void TUI::run() {
    Simulation simulation;
    auto screen = App::Fullscreen();
    std::atomic_bool running = false;
    bool commandError = false;
    std::string lastSignal = "尚未收到时钟信号";
    std::string command;
    std::string feedback = "输入命令后按 Enter";

    std::vector<std::string> processNames;
    int selected = 0;
    MenuOption menuOptions;
    menuOptions.entries_option.transform = [&](const EntryState& entry) {
        const auto& process = simulation.processes()[entry.index];
        const auto& pcb = process.pcb;
        auto row =
            processRow(std::to_string(pcb.getPid()), entry.label, processState(pcb),
                       pcb.isTerminated() ? "—" : "Q" + std::to_string(process.queueLevel),
                       std::to_string(pcb.getCpuTime()), std::to_string(pcb.getRemainingTime()));
        return entry.focused ? row | inverted : row;
    };
    auto processMenu = Menu(&processNames, &selected, menuOptions);

    const auto advance = [&] {
        try {
            lastSignal = "信号 · " + describeTick(simulation.step());
        } catch (const std::overflow_error&) {
            running = false;
            feedback = "模拟时钟已达到上限";
            commandError = true;
        }
    };

    InputOption inputOptions;
    inputOptions.multiline = false;
    inputOptions.on_enter = [&] {
        std::istringstream arguments(std::exchange(command, ""));
        std::string action;
        if (!(arguments >> action)) {
            return;
        }
        commandError = false;
        if (action == "add") {
            std::string name;
            std::string workload;
            std::string extra;
            arguments >> name >> workload;
            int burstTime = 0;
            const auto end = workload.data() + workload.size();
            const auto parsed = std::from_chars(workload.data(), end, burstTime);
            if (name.empty() || parsed.ec != std::errc{} || parsed.ptr != end || burstTime <= 0 ||
                arguments >> extra) {
                feedback = "用法: add <进程名> <正整数 tick>";
                commandError = true;
                return;
            }
            const auto& process = simulation.addProcess(std::move(name), burstTime);
            processNames.push_back(process.pcb.getName());
            feedback = "已创建 PID " + std::to_string(process.pcb.getPid()) + "，进入 Q1";
            return;
        }
        std::string extra;
        if (arguments >> extra) {
            feedback = "该命令不接收额外参数";
            commandError = true;
        } else if (action == "step") {
            running = false;
            feedback = "手动推进一拍";
            advance();
        } else if (action == "run") {
            running = true;
            feedback = "已开启自动时钟";
        } else if (action == "pause") {
            running = false;
            feedback = "已暂停自动时钟";
        } else if (action == "quit") {
            screen.Exit();
        } else {
            feedback = "未知命令: " + action;
            commandError = true;
        }
    };
    auto input = Input(&command, "输入命令", inputOptions);
    auto layout = Container::Vertical({input, processMenu});
    auto dashboard = Renderer(layout, [&] {
        const auto rows = processNames.empty()
                              ? text("暂无进程，输入 add 创建") | flex
                              : processMenu->Render() | vscroll_indicator | yframe | flex;
        return vbox({
            text(" OSDesign  操作系统模拟器") | bold | color(Color::Cyan),
            hbox({clockPanel(simulation.time(), running, lastSignal) | flex,
                  cpuPanel(simulation.cpu()) | flex}),
            window(
                text("进程 · MLFQ") | bold,
                vbox({
                    hbox({processRow("PID", "名称", text("状态"), "队列", "已运行", "剩余") | xflex,
                          text(" ")}) |
                        bold,
                    separator(),
                    rows,
                })) |
                flex,
            text(feedback) | color(commandError ? Color::Yellow : Color::Cyan),
            hbox({text("> ") | bold, input->Render() | flex}) | borderRounded,
            hbox({text(" add <名称> <tick> · step · run · pause · quit"), filler(),
                  text("Tab 切换 · ↑↓ 浏览 ")}),
        });
    });
    auto root = CatchEvent(dashboard, [&](Event event) {
        // 菜单默认用 Tab 换行，这里用 Tab 在两个区域之间切换。
        if (event == Event::Tab || event == Event::TabReverse) {
            if (input->Focused() && !processNames.empty()) {
                processMenu->TakeFocus();
            } else {
                input->TakeFocus();
            }
            return true;
        }
        if (event != Event::Custom) {
            return false;
        }
        if (running) {
            advance();
        }
        return true;
    });

    // 自动时钟只投递事件，命令和每个节拍都在界面线程更新模拟状态。
    std::jthread ticker([&screen, &running](std::stop_token stop) {
        std::mutex mutex;
        std::condition_variable_any wake;
        std::unique_lock lock(mutex);
        while (!stop.stop_requested()) {
            wake.wait_for(lock, stop, kTickInterval, [] { return false; });
            if (!stop.stop_requested() && running) {
                screen.Post(Event::Custom);
            }
        }
    });
    screen.Loop(root);
}
