#pragma once

#include <chrono>
#include <iostream>
#include <sstream>
#include <utility>

// A tiny header-only logger. The plan rules out spdlog, so this is the whole of
// it: variadic, prints to std::cerr (keeping std::cout free for actual program
// output), and ships a small stopwatch for human-friendly timing lines.
namespace graphene::log {

// Print one "[graphene] ..." line. Arguments are streamed in order, so call it
// like: log::info("loaded ", n, " nodes in ", ms, " ms").
template <typename... Args>
inline void info(Args&&... args) {
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    std::cerr << "[graphene] " << oss.str() << '\n';
}

// Monotonic stopwatch for measuring how long a step took.
class Stopwatch {
public:
    Stopwatch() : start_(std::chrono::steady_clock::now()) {}

    void reset() { start_ = std::chrono::steady_clock::now(); }

    // Elapsed milliseconds since construction / last reset.
    double ms() const {
        const auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

}  // namespace graphene::log
