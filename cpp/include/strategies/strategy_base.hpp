#pragma once

#include <vector>
#include <span>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <optional>
#include <iostream>
#include <type_traits>

#include <atomic>

#include "core/market_events.hpp"
#include "core/order_events.hpp"
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


/*
 * SIGNAL semantics:
 *
 * Strict (rejected if state mismatch):
 *   LONG   → open long position    (valid when open_position >= 0)
 *   SHORT  → open short position   (valid when open_position <= 0)
 *   SELL   → close long position   (valid when open_position > 0)
 *   COVER  → close short position  (valid when open_position < 0)
 *
 * Blind (state-blind, just transact qty in this direction):
 *   BBUY   → buy  qty: position += qty  (scales in, covers shorts, flips if qty > |pos|)
 *   BSELL  → sell qty: position -= qty
 *
 *   FLAT   → no-op
 *
 * Invalid strict signals are logged as REJECTED orders. Blind signals never internally reject based on direction reasons.
 */
enum class SIGNAL {
    SELL = -2,
    SHORT = -1,
    FLAT = 0,
    LONG = 1,
    COVER = 2,

    BBUY = 9,
    BSELL = -9,
};
std::ostream& operator<<(std::ostream& os, SIGNAL sig);

struct StrategyConfig {
    bool active = true;             // allows trades
    bool long_active = true;        // allows longs
    bool short_active = true;       // allows shorts
    bool stacking = true;           // allows stacking orders in the same direction

    TS_UNIT     ts_unit     = TS_UNIT::MILLISECONDS;
    DATE_FORMAT date_format = DATE_FORMAT::DDMMYYYY;
};


struct SharedStrategyBase {
    inline static std::atomic<long long> id_state = 0;
};
template <typename Tin>
class Strategy : public SharedStrategyBase {
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

    // works for: Live and Backtest -- generates a OrderEvent action event
    virtual void on_data(const Tin& input, std::vector<OrderEvent>& out) = 0;

    // works for backtest
    void on_bt_data(std::span<const Tin> inputs, std::vector<std::vector<OrderEvent>>& out) {
        out.clear();
        out.resize(inputs.size());

        for (std::size_t i=0; i < inputs.size(); ++i) {
            on_data(inputs[i], out[i]);
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

    // base config
    StrategyConfig bconfig;

    // resolves the right timestamp type at compile time
    long long resolve_ts(const Tin& input) {
        auto ts = get_timestamp(input);

        if constexpr (std::is_same_v<decltype(ts), std::string>) {
            return to_epoch_ms(ts, bconfig.date_format);
        }
        else if constexpr (std::is_integral_v<decltype(ts)>) {
            return to_epoch_ms(ts, bconfig.ts_unit);
        }
        else {
            static_assert(false, "resolve_ts: timestamp field type not supported");
        }
    }
};



