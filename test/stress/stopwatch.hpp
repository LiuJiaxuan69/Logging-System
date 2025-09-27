#pragma once

#include <chrono>
#include <string>
#include <iostream>
#include <fstream>

using steady_clock_t = std::chrono::steady_clock;

class Stopwatch {
public:
    Stopwatch(bool start_now = true) {
        if (start_now) restart();
    }
    void restart() {
        _start = _last = steady_clock_t::now();
    }
    // 上一次 lap 到现在的间隔（毫秒），并将“上一次”推进到现在
    double lap_ms() {
        auto now = steady_clock_t::now();
        auto ms = std::chrono::duration<double, std::milli>(now - _last).count();
        _last = now;
        return ms;
    }
    // 自启动到现在的总用时（不会更新“上一次”）
    double elapsed_ms() const {
        auto now = steady_clock_t::now();
        return std::chrono::duration<double, std::milli>(now - _last).count();
    }
    double elapsed_sec() const { return elapsed_ms() / 1000.0; }
private:
    steady_clock_t::time_point _start{};
    steady_clock_t::time_point _last{};
};

// 作用域计时器：析构时输出耗时；可把结果写回外部变量
class ScopedTimer {
public:
    ScopedTimer(const std::string &name, std::ostream& os = std::cout, double* out_ms = nullptr)
    : _name(name), _os(os), _out_ms(out_ms) { _sw.restart(); }
    ~ScopedTimer() noexcept {
        double ms = _sw.elapsed_ms();
        if(_out_ms) *_out_ms = ms;
        _os << "[TIMER] " << _name << ": " << ms << "ms\n";
    }
private:
    std::string _name;
    std::ostream& _os;
    Stopwatch _sw{true};
    double* _out_ms;
};

// 简单吞吐工具：根据总次数与耗时计算速率
inline double ops_per_sec(uint64_t total_ops, double elapsed_ms) {
    if (elapsed_ms <= 0) return 0.0;
    return static_cast<double>(total_ops) / (elapsed_ms / 1000.0);
}
inline double mb_per_sec(uint64_t total_bytes, double elapsed_ms) {
    if (elapsed_ms <= 0) return 0.0;
    return (static_cast<double>(total_bytes) / (1024.0 * 1024.0)) / (elapsed_ms / 1000.0);
}