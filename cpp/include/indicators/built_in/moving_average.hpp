#pragma once

#include <stdexcept>
#include <vector>
#include <optional>
#include <cstddef>

#include "indicators/indicator_base.hpp"



template <typename Tin, typename Tout>
class SMA : public Indicator<SMA<Tin, Tout>, Tin, Tout> {
public:
    explicit SMA(std::size_t len_) : len(len_) {
        if (len_ == 0) {throw std::invalid_argument("SMA len must be > 0");}
        history.reserve(len);
    }


    std::optional<Tout> compute(const Tin& input) {
        const Tout val = static_cast<Tout>(input);

        //resize history and calc the sum of the new element if full
        if (history.size() < len) {
            history.push_back(val);
            sum += val;
        } else {
            sum -= history[idx];
            history[idx] = val;
            sum += val;
        }

        idx = (++idx) % len;

        if (history.size() != len) {return std::nullopt;}
        return static_cast<Tout>(sum / history.size());
    }


    // getters
    std::size_t get_len() const  { return len; }

    // setters
    void set_len(std::size_t len_) {
        if (len_ == 0) {throw std::invalid_argument("SMA len must be > 0");}
        len = len_;
        history.clear();
        history.reserve(len);
        this->buffer.clear();
        this->buffer.reserve(64);
        idx = 0;
        sum = 0.0;
    }

private:
    std::size_t len;
    std::vector<Tout> history;

    std::size_t idx = 0;
    double sum{};

};