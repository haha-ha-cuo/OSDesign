//
// Created by YuHao Shi on 2026/11/13.
//

#include "../include/CLOCK.h"

#include <limits>
#include <stdexcept>

CLOCK::CLOCK(int initialTime) : currentTime_(initialTime) {
    if (initialTime < 0) {
        throw std::invalid_argument("Initial clock time cannot be negative");
    }
}

int CLOCK::advance(int ticks) {
    if (ticks <= 0) {
        throw std::invalid_argument("Clock advance must be positive");
    }
    if (ticks > std::numeric_limits<int>::max() - currentTime_) {
        throw std::overflow_error("Simulation clock overflow");
    }
    currentTime_ += ticks;
    return currentTime_;
}

void CLOCK::reset(int initialTime) {
    if (initialTime < 0) {
        throw std::invalid_argument("Initial clock time cannot be negative");
    }
    currentTime_ = initialTime;
}
