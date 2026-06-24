#pragma once

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "utils/time.hpp"


enum class DATA_SPLIT_SIDE {
    FIRST,
    LAST
};


inline std::string date_string(long long ts) {
    const auto tm = localtime_safe(static_cast<std::time_t>(ts / 1000));
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d");
    return out.str();
}


template <typename Tin>
std::string first_date(const std::vector<Tin>& data) {
    if (data.empty()) {
        return "";
    }

    return date_string(get_timestamp(data.front()));
}


template <typename Tin>
std::string last_date(const std::vector<Tin>& data) {
    if (data.empty()) {
        return "";
    }

    return date_string(get_timestamp(data.back()));
}


template <typename Tin>
std::vector<Tin> filter_by_date(
    const std::vector<Tin>& data,
    const std::string& start_date,
    const std::string& end_date
) {
    const long long start_ts = start_date.empty() ? std::numeric_limits<long long>::min() : to_epoch_ms(start_date);
    const long long end_ts = end_date.empty() ? std::numeric_limits<long long>::max() : to_epoch_ms(end_date);

    std::vector<Tin> out;
    out.reserve(data.size());

    for (const auto& row : data) {
        const long long ts = get_timestamp(row);
        if (ts >= start_ts && ts <= end_ts) {
            out.push_back(row);
        }
    }

    return out;
}


template <typename Tin>
std::vector<Tin> split_by_pct(
    const std::vector<Tin>& data,
    double pct,
    DATA_SPLIT_SIDE side
) {
    // safety check
    if (pct <= 0.0 || pct > 1.0) {
        throw std::invalid_argument("split pct must be in (0, 1]");
    }

    const auto n = data.size();
    const auto split_n = static_cast<std::size_t>(std::ceil(static_cast<double>(n) * pct));

    if (side == DATA_SPLIT_SIDE::LAST) {
        const auto start_idx = n > split_n ? n - split_n : 0;
        return std::vector<Tin>(data.begin() + start_idx, data.end());
    }

    const auto end_idx = std::min(split_n, n);
    return std::vector<Tin>(data.begin(), data.begin() + end_idx);
}
