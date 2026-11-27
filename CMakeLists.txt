cmake_minimum_required(VERSION 3.20)
project(OSDesign)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MinGW 默认动态链接 libstdc++/libgcc，直接运行产物会因找不到这些 DLL 而启动失败。
# 这里静态链接这两个运行库；libwinpthread 在该工具链下无法静态链接，改为复制到产物旁边。
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++)
endif()

set(PROJECT_SOURCES
        src/main.cpp
        src/PCB.cpp
        include/PCB.h
        src/MLFQ.cpp
        include/MLFQ.h
        src/CLOCK.cpp
        include/CLOCK.h
        src/CPU.cpp
        include/CPU.h
        include/PS.h
        src/PS.cpp
        src/Simulation.cpp
        include/Simulation.h
        src/TUI.cpp
        include/TUI.h)

include(FetchContent)
FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v7.0.0
        GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(ftxui)

find_package(Threads REQUIRED)
add_executable(OSDesign ${PROJECT_SOURCES})
target_link_libraries(OSDesign PRIVATE ftxui::ftxui Threads::Threads)

# 把 MinGW 仍需动态加载的运行库复制到可执行文件旁，使 exe 可以直接双击运行。
if(MINGW)
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(RUNTIME_DLL libwinpthread-1.dll libgcc_s_seh-1.dll libstdc++-6.dll)
        if(EXISTS "${MINGW_BIN_DIR}/${RUNTIME_DLL}")
            add_custom_command(TARGET OSDesign POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MINGW_BIN_DIR}/${RUNTIME_DLL}"
                    "$<TARGET_FILE_DIR:OSDesign>"
                    COMMENT "Copying ${RUNTIME_DLL} next to OSDesign.exe")
        endif()
    endforeach()
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME SchedulingDemo COMMAND OSDesign)

    add_executable(ProcessControlBlockTest
            tests/ProcessControlBlockTest.cpp
            src/PCB.cpp)
    add_test(NAME ProcessControlBlockTest COMMAND ProcessControlBlockTest)

    add_executable(CLOCKTest
            tests/CLOCKTest.cpp
            src/CLOCK.cpp
            src/PCB.cpp)
    add_test(NAME CLOCKTest COMMAND CLOCKTest)

    add_executable(CPUTest
            tests/CPUTest.cpp
            src/CPU.cpp
            src/CLOCK.cpp
            src/PCB.cpp)
    add_test(NAME CPUTest COMMAND CPUTest)

    add_executable(MLFQTest
            tests/MLFQTest.cpp
            src/MLFQ.cpp
            src/CPU.cpp
            src/CLOCK.cpp
            src/PCB.cpp)
    add_test(NAME MLFQTest COMMAND MLFQTest)

    add_executable(PSTest
            tests/PSTest.cpp
            src/PS.cpp
            src/CPU.cpp
            src/CLOCK.cpp
            src/PCB.cpp)
    add_test(NAME PSTest COMMAND PSTest)

    add_executable(SimulationTest
            tests/SimulationTest.cpp
            src/Simulation.cpp
            src/MLFQ.cpp
            src/CPU.cpp
            src/CLOCK.cpp
            src/PCB.cpp)
    add_test(NAME SimulationTest COMMAND SimulationTest)
endif()

# 代码格式化：优先使用 -DCLANG_FORMAT_EXECUTABLE=<path> 指定的版本，
# 其次在 PATH 中查找；macOS 上再回退到 Xcode 自带的 clang-format。
if(NOT CLANG_FORMAT_EXECUTABLE)
    find_program(CLANG_FORMAT_EXECUTABLE
            NAMES clang-format clang-format-22 clang-format-21 clang-format-20
            clang-format-19 clang-format-18 clang-format-17
            DOC "Path to the clang-format executable")
endif()

if(NOT CLANG_FORMAT_EXECUTABLE AND APPLE)
    execute_process(
            COMMAND xcrun --find clang-format
            OUTPUT_VARIABLE CLANG_FORMAT_FROM_XCRUN
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE CLANG_FORMAT_XCRUN_RESULT)
    if(CLANG_FORMAT_XCRUN_RESULT EQUAL 0 AND EXISTS "${CLANG_FORMAT_FROM_XCRUN}")
        set(CLANG_FORMAT_EXECUTABLE "${CLANG_FORMAT_FROM_XCRUN}"
                CACHE FILEPATH "Path to the clang-format executable" FORCE)
    endif()
endif()

file(GLOB_RECURSE FORMAT_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
list(SORT FORMAT_SOURCES)

if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format
            COMMAND "${CLANG_FORMAT_EXECUTABLE}" --style=file -i ${FORMAT_SOURCES}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Formatting C++ sources with clang-format"
            VERBATIM)

    add_custom_target(format-check
            COMMAND "${CLANG_FORMAT_EXECUTABLE}" --style=file --dry-run --Werror ${FORMAT_SOURCES}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Checking C++ formatting with clang-format"
            VERBATIM)
else()
    message(WARNING
            "clang-format not found. Install it or configure with "
            "-DCLANG_FORMAT_EXECUTABLE=<path> to enable the format targets.")
    add_custom_target(format
            COMMAND ${CMAKE_COMMAND} -E echo
            "clang-format not found; install it or pass -DCLANG_FORMAT_EXECUTABLE=<path>."
            COMMAND ${CMAKE_COMMAND} -E false
            VERBATIM)
endif()
