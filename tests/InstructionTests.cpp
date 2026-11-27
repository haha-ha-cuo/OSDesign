#include "Core.h"
#include "Instruction.h"
#include "Process.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

int main() {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("osdesign-instructions-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if (!std::filesystem::create_directory(directory)) {
        std::cerr << "无法创建测试目录\n";
        return 1;
    }

    try {
        const std::vector<std::pair<std::string, Instruction>> valid{
            {"COMPUTE 5", {Instruction::COMPUTE, 5}}, {"\tIO 3\r", {Instruction::IO, 3}},
            {"READ 2", {Instruction::READ, 2}},       {"YIELD", {Instruction::Yield, 0}},
            {"EXIT 0", {Instruction::Exit, 0}},       {"EXIT -1", {Instruction::Exit, -1}},
        };
        for (const auto& [line, expected] : valid) {
            const auto actual = Instruction::parseInstruction(line);
            if (actual.type != expected.type || actual.argument != expected.argument) {
                throw std::runtime_error("解析结果错误：" + line);
            }
        }

        for (const std::string line :
             {"", "UNKNOWN 1", "COMPUTE", "IO 0", "READ -1", "COMPUTE 1.5",
              "COMPUTE 999999999999999999999", "EXIT text", "EXIT", "YIELD 1", "COMPUTE 2 extra"}) {
            bool rejected = false;
            try {
                Instruction::parseInstruction(line);
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            if (!rejected) {
                throw std::runtime_error("未拒绝非法指令：" + line);
            }
        }

        Core core;
        Process process;
        const auto path = directory / "program.txt";
        {
            std::ofstream file(path, std::ios::binary);
            file << " \t\r\nCOMPUTE 5\r\nIO 3\r\n\nREAD 2\r\nYIELD\r\nEXIT 0";
        }
        if (core.loadfile(&process, path.string()) != Core::SUCCESS) {
            throw std::runtime_error("无法加载包含空白行、CRLF 和无末尾换行的程序");
        }
        if (core.loadfile(nullptr, path.string()) != Core::FAILURE ||
            core.loadfile(&process, (directory / "missing.txt").string()) != Core::FAILURE) {
            throw std::runtime_error("未拒绝空指针或不存在的文件");
        }

        for (const std::string content : {"", " \t\n\r\n", "COMPUTE 2\nBAD 3\n"}) {
            {
                std::ofstream file(path);
                file << content;
            }
            if (core.loadfile(&process, path.string()) != Core::FAILURE) {
                throw std::runtime_error("未拒绝空程序或格式错误的程序");
            }
        }

        {
            std::ofstream file(path);
            file << "COMPUTE 1\nEXIT 0\n";
        }
        if (core.loadfile(&process, path.string()) != Core::SUCCESS) {
            throw std::runtime_error("失败后无法重新加载合法程序");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::filesystem::remove_all(directory);
        return 1;
    }

    std::filesystem::remove_all(directory);
    return 0;
}
