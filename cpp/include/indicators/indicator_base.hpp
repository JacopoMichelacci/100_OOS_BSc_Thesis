#pragma once

#include <cstddef>
#include <vector>
#include <span>
#include <optional>
#include <utility>
#include <type_traits>
#include <string_view>
#include <iostream>
#include <cmath>

#include "core/market_events.hpp"



template <typename Derived, typename Tin, typename Tout>
class Indicator {
public:
    ~Indicator() = default;

    // returns new indicator value
    std::optional<Tout> update(const Tin& input) {
        return derived().compute(input);
    }

    // computes new ind value and updates buffer
    void update_buffer(const Tin& input) {
        buffer.emplace_back(derived().compute(input));
    }

    // handle buffer operations
    // index --- works wiht negative indexing
    std::optional<Tout> operator[](int i) const {
        if (buffer.empty()) { return std::nullopt; }
        int buffsize = buffer.size();

        if (i >= 0 && i < buffsize) {
            return buffer[i];
        }
        else if (i < 0) {
            if (-i <= buffsize) {
                return buffer[buffsize + i];
            }
        }

        return std::nullopt;
    }

    // get size
    int size() const { return static_cast<int>(buffer.size()); }

    // view buffer
    std::span<const std::optional<Tout>> view_buffer() const { return buffer; }


protected:
    // storage of past indicator values in time idx order  ( 0 -> first; .size() -1 -> last )
    std::vector<std::optional<Tout>> buffer;

    // constructor
    Indicator(std::size_t buffer_len_ = 64) {
        // check the compute function in Derived is valid
        static_assert(
            std::is_same_v<decltype(std::declval<Derived&>().compute(std::declval<const Tin&>())),
            std::optional<Tout>>,
            "Derived must implement: std::optional<Tout> compute(const Tin&)"
        );

        buffer.reserve(buffer_len_);
    }

private:
    // CRTP
    Derived& derived() {
        return static_cast<Derived&>(*this);
    }
    const Derived& derived() const {
        return static_cast<const Derived&>(*this);
    }
};