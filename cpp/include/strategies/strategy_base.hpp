#pragma once

#include <vector>
#include <span>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <optional>
#include <iostream>

#include "core/market_events.hpp"
#include "utils/time.hpp"


namespace detail {
    std::string resolve_name(const std::string& name, const std::string& file_path) {
        if (!name.empty()) {
            return name; 
        }

        const auto slash = file_path.find_last_of("/\\");
        const auto file  = (slash == std::string::npos) ? file_path : file_path.substr(slash + 1);
        const auto dot   = file.find_last_of('.');
        return (dot == std::string::npos) ? file : file.substr(0, dot);
    }
}


enum class Signal {
    SELL = -2,
    SHORT = -1,
    FLAT = 0,
    LONG = 1,
    COVER = 2,
};

struct StrategyConfig {
    bool active = true;
    bool long_active = true;
    bool short_active = true;
};

template <typename Tin>
class Strategy {
public:
    Strategy(std::string name_, StrategyConfig bconfig_ = {}) 
        : name(std::move(name_)), 
        bconfig(std::move(bconfig_)),
        id(++id_state), 
        tag(name + "#" + std::to_string(id)) {
            if (name.empty()) {
                std::cerr << "Strategy with id: " << id << " was neglected a meaningful name.\n";
            }
        }

    virtual ~Strategy() = default;

    // works for: Live and Backtest -- generates a signal action event
    virtual Signal on_data(const Tin& input) = 0;

    // works for backtest
    void on_bt_data(std::span<const Tin> inputs, std::vector<Signal>& out) {
        out.clear();
        out.reserve(inputs.size());

        for (const auto& in : inputs) {
            out.push_back(on_data(in));
        }
    }


    // getters
    std::string_view get_name() const { return name; }
    long long get_id() const { return id; }
    std::string_view get_tag() const { return tag; }


protected:
    std::string name;
    long long id;
    std::string tag;

    StrategyConfig bconfig;

private:
    inline static long long id_state = 0;
};



