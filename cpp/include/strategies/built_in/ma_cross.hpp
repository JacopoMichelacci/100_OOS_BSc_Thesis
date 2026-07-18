#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>
#include <string>

#include "strategies/strategy_base.hpp"
#include "core/order_events.hpp"
#include "core/price_field.hpp"
#include "indicators/built_in/moving_average.hpp"

template <typename Tin>
struct MACConfig {
    int16_t fast_len = 10;
    PRICE_FIELD fast_price_field = PRICE_FIELD::CLOSE;

    int16_t slow_len = 30;
    PRICE_FIELD slow_price_field = PRICE_FIELD::CLOSE;

    double slnot = -1.0;
    double slpct = -1.0;  // percent, e.g. 2.0 = 2%; negative disables

    SIZING_MODE pos_sizing_mode = SIZING_MODE::FIXED;
    double qty = 1.0;
    double equity_pct = 0.05;
};

template <typename Tin>
class MAC : public Strategy<Tin> {
public:
    MAC(std::string name_, const MACConfig<Tin>& params_, StrategyConfig base_config_ = {}) 
        : Strategy<Tin>(detail::resolve_name(name_, __FILE__), base_config_),
        params(params_),

        fast_sma(params_.fast_len, 2),
        slow_sma(params_.slow_len, 2) {

        //safety checks
        if (params.fast_len >= params.slow_len || params.fast_len < 1 || params.slow_len < 1) {
            throw std::invalid_argument("lengths must be positive. fast length must be < slow length");
        }

        if (params.equity_pct <= 0.0 || params.equity_pct > 1.0) {
            throw std::invalid_argument("MAC equity_pct must be in (0, 1]");
        }
    }

    virtual void on_data(const Tin& input, const StrategyContext& ctx, std::vector<OrderEvent>& out) override {
        tc.update(this->resolve_ts(input));

        if (!this->bconfig.active) {
            return;
        }

        const double fast_price = get_price_field(input, params.fast_price_field);
        const double slow_price = get_price_field(input, params.slow_price_field);

        // update indicators
        fast_sma.update_buffer(fast_price);
        slow_sma.update_buffer(slow_price);

        // check we have data valid ma's
        if (!slow_sma[-1] || !slow_sma[-2]) {
            return;
        }

        // LONG 
        if (this->bconfig.long_active) {
            if (fast_sma[-1] >= slow_sma[-1] && fast_sma[-2] < slow_sma[-2]) {
                if (!this->bconfig.stacking && ctx.opos > 0.0) {
                    return;
                }
                const double target_qty = calculate_qty(ctx, input.close);
                const double buy_qty = ctx.opos < 0.0 ? target_qty - ctx.opos : target_qty;
                out.push_back({.signal = SIGNAL::BBUY, .type = ORDER_TYPE::MARKET, .qty = buy_qty,
                    .sl = StopLoss{.type = STOP_TYPE::HARD, .slnot = params.slnot, .slpct = params.slpct}});
            }
        }


        // SHORT
        if (this->bconfig.short_active) {
            if (fast_sma[-1] <= slow_sma[-1] && fast_sma[-2] > slow_sma[-2]) {
                if (!this->bconfig.stacking && ctx.opos < 0.0) {
                    return;
                }
                const double target_qty = calculate_qty(ctx, input.close);
                const double sell_qty = ctx.opos > 0.0 ? target_qty + ctx.opos : target_qty;
                out.push_back({.signal = SIGNAL::BSELL, .type = ORDER_TYPE::MARKET, .qty = sell_qty,
                    .sl = StopLoss{.type = STOP_TYPE::HARD, .slnot = params.slnot, .slpct = params.slpct}});
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
                throw std::invalid_argument("MAC unsupported sizing mode");
        }
    }

private:
    MACConfig<Tin> params;

    SMA<double, double> fast_sma;
    SMA<double, double> slow_sma;

    TimeConverter tc = {};
    
};
