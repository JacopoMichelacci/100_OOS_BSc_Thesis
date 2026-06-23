#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>
#include <string>

#include "strategies/strategy_base.hpp"
#include "core/order_events.hpp"
#include "indicators/built_in/standard_deviation.hpp"
#include "utils/field_access.hpp"


template <typename Tin>
struct Std_MrevConfig {
    int std_len = 15;
    double Tin::* field = default_close_field<Tin, double>();
    double lower_std_thresh = -1.5;
    double upper_std_thresh = 1.5;


    SIZING_MODE pos_sizing_mode = SIZING_MODE::FIXED_FRACTIONAL_PRICE;
    double qty = 1.0;
    double equity_pct = 0.01;
};


template <typename Tin>
class Std_MRev : public Strategy<Tin> {
public:
    Std_MRev(std::string name_, const Std_MrevConfig<Tin>& params_, StrategyConfig base_config_ = {})
        : Strategy<Tin>(detail::resolve_name(name_, __FILE__), base_config_),
        params(params_),

        stddev(params_.std_len, 4) {

        //safety checks
        if (params.std_len < 2) {
            throw std::invalid_argument("Std_MRev std_len must be at least 2");
        }

        if (params.qty <= 0.0) {
            throw std::invalid_argument("Std_MRev qty must be positive");
        }
        if (params.equity_pct <= 0.0 || params.equity_pct > 1.0) {
            throw std::invalid_argument("Std_MRev equity_pct must be in (0, 1]");
        }
        if (params.field == nullptr) {
            throw std::invalid_argument("Std_MrevConfig requires field when Tin has no double close field");
        }
    }

    virtual void on_data(const Tin& input, const StrategyContext& ctx, std::vector<OrderEvent>& out) override {
        tc.update(this->resolve_ts(input));

        if (!this->bconfig.active) {
            return;
        }
        
        const double value = input.*params.field;
        if (!prev_value) {
            prev_value = value;
            return;
        }

        // calc last move
        const double last_move = value - *prev_value;
        prev_value = value;

        stddev.update_buffer(last_move);

        // check if value exist and is != 0
        const auto rolling_std = stddev[-1];
        if (!rolling_std || *rolling_std == 0.0) {
            return;
        }

        const double standardized_move = last_move / *rolling_std;

        // LONG
        if (this->bconfig.long_active) {
            if (standardized_move <= params.lower_std_thresh) {
                if (!this->bconfig.stacking && ctx.opos > 0.0) {
                    return;
                }
                const double target_qty = calculate_qty(ctx, value);
                const double buy_qty = ctx.opos < 0.0 ? target_qty - ctx.opos : target_qty;
                if (buy_qty > 0.0) {
                    out.push_back({.signal = SIGNAL::BBUY, .qty = buy_qty, .type = ORDER_TYPE::MARKET});
                }
            }
        }

        // SHORT
        if (this->bconfig.short_active) {
            if (standardized_move >= params.upper_std_thresh) {
                if (!this->bconfig.stacking && ctx.opos < 0.0) {
                    return;
                }
                const double target_qty = calculate_qty(ctx, value);
                const double sell_qty = ctx.opos > 0.0 ? target_qty + ctx.opos : target_qty;
                if (sell_qty > 0.0) {
                    out.push_back({.signal = SIGNAL::BSELL, .qty = sell_qty, .type = ORDER_TYPE::MARKET});
                }
            }
        }
    }

    double calculate_qty(const StrategyContext& ctx, double price) const {
        switch (params.pos_sizing_mode) {
            case SIZING_MODE::FIXED:
                return params.qty;
            case SIZING_MODE::FIXED_FRACTIONAL_PRICE:
                return this->sizer.fixed_fractional_price(ctx.equity, price, params.equity_pct);
            default:
                throw std::invalid_argument("Std_MRev unsupported sizing mode");
        }
    }

private:
    Std_MrevConfig<Tin> params;

    STD<double, double> stddev;

    TimeConverter tc{};
    std::optional<double> prev_value;
};
