#pragma once

#include <vector>
#include <string>

#include "strategies/strategy_base.hpp"
#include "core/order_events.hpp"
#include "core/market_events.hpp"
#include "indicators/built_in/moving_average.hpp"

struct MACConfig {
    int16_t fast_len = 10;
    PRICE_FIELD fast_field = PRICE_FIELD::CLOSE;

    int16_t slow_len = 30;
    PRICE_FIELD slow_field = PRICE_FIELD::CLOSE;

    double qty = 1.0;
};

template <typename Tin>
class MAC : public Strategy<Tin> {
public:
    MAC(std::string name_, const MACConfig& params_, StrategyConfig base_config_ = {}) 
        : Strategy<Tin>(detail::resolve_name(name_, __FILE__), base_config_),
        params(params_),

        fast_sma(params_.fast_len),
        slow_sma(params_.slow_len) {

        //safety checks
        if (params.fast_len >= params.slow_len || params.fast_len < 1 || params.slow_len < 1) {
            throw std::invalid_argument("lengths must be positive. fast length must be < slow length");
        }
    }

    virtual void on_data(const Tin& input, std::vector<OrderEvent>& out) override {
        tc.update(this->resolve_ts(input));

        fast_sma.update_buffer(input.close);
        slow_sma.update_buffer(input.close);

        // check we have data valid ma's
        if (!slow_sma[-1] || !slow_sma[-2]) {
            return;
        }

        // LONG 
        if (this->bconfig.long_active) {
            if (fast_sma[-1] >= slow_sma[-1] && fast_sma[-2] < slow_sma[-2]) {
                out.push_back({.signal = SIGNAL::BBUY, .qty = params.qty, .type = ORDER_TYPE::MARKET});
            }
        }

        // SHORT
        if (this->bconfig.short_active) {
            if (fast_sma[-1] <= slow_sma[-1] && fast_sma[-2] > slow_sma[-2]) {
                out.push_back({.signal = SIGNAL::BSELL, .qty = params.qty, .type = ORDER_TYPE::MARKET});
            }
        }

        
    }

private:
    MACConfig params;

    SMA<double, double> fast_sma;
    SMA<double, double> slow_sma;

    TimeConverter tc = {};
    
};
