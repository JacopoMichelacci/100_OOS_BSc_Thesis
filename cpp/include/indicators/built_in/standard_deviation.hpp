#pragma once

#include <stdexcept>
#include <vector>
#include <optional>
#include <cstddef>
#include <cmath>

#include "indicators/indicator_base.hpp"



template <typename Tin, typename Tout>
class STD : public Indicator<STD<Tin, Tout>, Tin, Tout> {
public:
    explicit STD(int len_, int max_buffer_size_ = -1)
        : Indicator<STD<Tin, Tout>, Tin, Tout>(max_buffer_size_),
        len(len_) {
        // safety checks
        if (len_ < 1) {
            throw std::invalid_argument("std len must be > 0");
        }

        history.reserve(len);
    }

    std::optional<Tout> compute(const Tin& input) {
        const double val = static_cast<double>(input);
        const double sqval = val * val;

        if (history.size() < len) {
            history.push_back(input);
            sum += val;
            sqsum += sqval;
        } else {
            const double oldval = static_cast<double>(history[idx]);

            sum -= oldval;
            sqsum -= oldval * oldval;

            history[idx] = input;
            sum += val;
            sqsum += sqval;
        }

        ++idx;
        if (idx == len) { idx = 0; }

        if (history.size() != len) { return std::nullopt; }

        const double mean = sum / len;
        double variance = sqsum / len - mean * mean;

        // floating point rounding can produce a tiny negative variance
        if (variance < 0.0) { variance = 0.0; }

        return static_cast<Tout>(std::sqrt(variance));
    }


    // getters
    std::size_t get_len() const { return len; }

    // setters
    void set_len(std::size_t len_) {
        if (len_ == 0) {throw std::invalid_argument("STD len must be > 0");}
        len = len_;
        history.clear();
        history.reserve(len);
        this->reset_buffer();
        idx = 0;
        sum = 0.0;
        sqsum = 0.0;
    }

private:
    std::size_t len;
    std::vector<Tin> history;

    double sum = 0.0;
    double sqsum = 0.0;
    std::size_t idx = 0;
};
