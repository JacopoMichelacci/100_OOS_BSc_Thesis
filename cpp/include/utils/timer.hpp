#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <utility>
#include <cstddef>
#include <stdexcept>
#include <string_view>


enum class TIMER_TYPE : std::int8_t {
    MILLISECONDS,
    MICROSECONDS,
    NANOSECONDS,
};
inline std::ostream& operator<<(std::ostream& os, TIMER_TYPE type) {
    switch (type) {
        case (TIMER_TYPE::MILLISECONDS): return os << "milliseconds";
        case (TIMER_TYPE::MICROSECONDS): return os << "microseconds";
        case (TIMER_TYPE::NANOSECONDS): return os << "nanoseconds";
    }

    return os << "unknown";
}


class Timer {
public:

    Timer(TIMER_TYPE time_measure_type) 
        : start_(std::chrono::steady_clock::now()), time_measure_type_(time_measure_type) {}


    inline void start() {
        start_ = std::chrono::steady_clock::now();
    }

    inline long long end() {
        end_ = std::chrono::steady_clock::now();

        acc_time_ += time_measure_();
        return acc_time_;   
    }

    inline void reset() {
        acc_time_ = 0;
    }

    inline void print(std::string_view msg = "") const {
        if (msg == "") {
            std::cout << "Time elapsed: " << acc_time_ << " " << time_measure_type_ << "\n";
        }
        else {
            std::cout << msg << acc_time_ << time_measure_type_ << "\n";
        }
    }

    template <typename F>
    long long f_elapse(F&& f, std::size_t iter = 1) {
        if (iter < 1) {throw std::invalid_argument("iter function argument must be >= 1");}
        long long sum = 0;

        for (std::size_t i=0; i < iter; ++i) {
            start_ = std::chrono::steady_clock::now();
            f();
            end_ = std::chrono::steady_clock::now();

            sum += time_measure_();
        }

        return sum / iter;
    }

private:
    TIMER_TYPE time_measure_type_;                                 // unit measure of time for timer
    std::chrono::steady_clock::time_point start_;                   // start time of timer, = ::now()
    std::chrono::steady_clock::time_point end_;                     // end time of timer, = ::now()
    long long acc_time_ = 0;                                        // stores accumulated time for the timer


    inline long long time_measure_() {
        if (time_measure_type_ == TIMER_TYPE::MILLISECONDS) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(end_ - start_).count();
        }

        if (time_measure_type_ == TIMER_TYPE::MICROSECONDS) {
            return std::chrono::duration_cast<std::chrono::microseconds>(end_ - start_).count();
        }

        if (time_measure_type_ == TIMER_TYPE::NANOSECONDS) {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(end_ - start_).count();
        }

        throw std::invalid_argument("Error: must pass a valid argument for the timer time measure type.");
    }
};
