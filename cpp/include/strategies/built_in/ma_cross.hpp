#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>
#include <string>

#include "strategies/strategy_base.hpp"
#include "core/order_events.hpp"
#include "indicators/built_in/moving_average.hpp"
#include "utils/field_access.hpp"

template <typename Tin>
struct MACConfig {
    int16_t fast_len = 10;
    double Tin::* fast_field = default_close_field<Tin>();

    int16_t slow_len = 30;
    double Tin::* slow_field = default_close_field<Tin>();

    SIZING_MODE pos_sizing_mode = SIZING_MODE::FIXED;
    double qty = 1.0;
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

        if (params.fast_field == nullptr || params.slow_field == nullptr) {
            throw std::invalid_argument("MACConfig requires fast_field and slow_field when Tin has no double close field");
        }
    }

    virtual void on_data(const Tin& input, const StrategyContext& ctx, std::vector<OrderEvent>& out) override {
        tc.update(this->resolve_ts(input));

        fast_sma.update_buffer(input.*params.fast_field);
        slow_sma.update_buffer(input.*params.slow_field);

        // check we have data valid ma's
        if (!slow_sma[-1] || !slow_sma[-2]) {
            return;
        }

        // LONG 
        if (this->bconfig.long_active) {
            if (fast_sma[-1] >= slow_sma[-1] && fast_sma[-2] < slow_sma[-2]) {
                const double target_qty = calculate_qty(ctx, input);
                out.push_back({.signal = SIGNAL::BBUY, .qty = target_qty - ctx.opos, .type = ORDER_TYPE::MARKET});
            }
        }


        // SHORT
        if (this->bconfig.short_active) {
            if (fast_sma[-1] <= slow_sma[-1] && fast_sma[-2] > slow_sma[-2]) {
                const double target_qty = calculate_qty(ctx, input);
                out.push_back({.signal = SIGNAL::BSELL, .qty = target_qty + ctx.opos, .type = ORDER_TYPE::MARKET});
            }
        }

        
    }

    double calculate_qty(const StrategyContext& ctx, const Tin& input) const {
        switch (params.pos_sizing_mode) {
            case SIZING_MODE::FIXED:
                return params.qty;
            case SIZING_MODE::FIXED_FRACTIONAL_PRICE:
                return this->sizer.fixed_fractional_price(ctx.equity, input.*params.fast_field, params.qty);
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
