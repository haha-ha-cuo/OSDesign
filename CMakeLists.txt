cmake_minimum_required(VERSION 3.20)
project(OSDesign)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

add_executable(OSDesign
        main.cpp
        src/PCB.cpp
        src/FCB.cpp
        src/Process.cpp
        src/Dram.cpp
        src/Core.cpp
        src/CLOCK.cpp
        include/CLOCK.h
        src/CPU.cpp
        include/CPU.h
        src/IO.cpp
        include/IO.h
        src/Instruction.cpp
        include/Instruction.h
)

target_include_directories(OSDesign PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(BUILD_TESTING)
    add_executable(instruction_tests
            tests/InstructionTests.cpp
            src/Instruction.cpp
            src/Process.cpp
            src/Core.cpp
    )
    target_include_directories(instruction_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    add_test(NAME instruction_tests COMMAND instruction_tests)

    add_executable(cpu_tests
            tests/CPUTests.cpp
            src/CPU.cpp
    )
    target_include_directories(cpu_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    add_test(NAME cpu_tests COMMAND cpu_tests)

    add_executable(core_tests
            tests/CoreTests.cpp
            src/Core.cpp
            src/Process.cpp
            src/Instruction.cpp
    )
    target_include_directories(core_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    add_test(NAME core_tests COMMAND core_tests)
    add_test(NAME process_demo_test
            COMMAND OSDesign "${CMAKE_CURRENT_SOURCE_DIR}/examples/startup.txt")
    set_tests_properties(process_demo_test PROPERTIES
            PASS_REGULAR_EXPRESSION "SUMMARY state=RUNNING CPU=1 elapsed=1 remaining=2")
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
        "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
list(SORT FORMAT_SOURCES)

if(NOT FORMAT_SOURCES)
    add_custom_target(format
            COMMAND ${CMAKE_COMMAND} -E echo "No C++ sources to format.")
    add_custom_target(format-check
            COMMAND ${CMAKE_COMMAND} -E echo "No C++ sources to check.")
elseif(CLANG_FORMAT_EXECUTABLE)
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
