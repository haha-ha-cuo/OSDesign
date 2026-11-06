cmake_minimum_required(VERSION 3.20)
project(OSDesign)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_SOURCES
        src/main.cpp
        src/ProcessControlBlock.cpp
        include/ProcessControlBlock.h)

add_executable(OSDesign ${PROJECT_SOURCES})

include(CTest)
if(BUILD_TESTING)
    add_executable(ProcessControlBlockTest
            tests/ProcessControlBlockTest.cpp
            src/ProcessControlBlock.cpp)
    add_test(NAME ProcessControlBlockTest COMMAND ProcessControlBlockTest)
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
