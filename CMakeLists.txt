cmake_minimum_required(VERSION 3.20)
project(OSDesign)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(OSDesign src/main.cpp
        src/ProcessControlBlock.cpp
        include/ProcessControlBlock.h)

include(CTest)
if(BUILD_TESTING)
    add_executable(ProcessControlBlockTest
            tests/ProcessControlBlockTest.cpp
            src/ProcessControlBlock.cpp)
    add_test(NAME ProcessControlBlockTest COMMAND ProcessControlBlockTest)
endif()
