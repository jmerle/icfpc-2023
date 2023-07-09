#pragma once

#include <chrono>

class Timer {
    std::chrono::high_resolution_clock::time_point start;

public:
    Timer() {
        reset();
    }

    double elapsedSeconds() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::duration<double>>(now - start).count();
    }

    void reset() {
        start = std::chrono::high_resolution_clock::now();
    }
};
