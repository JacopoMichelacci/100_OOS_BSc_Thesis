#pragma once

#include <vector>
#include <string>

#include "strategies/strategy_base.hpp"
#include "indicators/built_in/moving_average.hpp"

struct MACConfig {
    int16_t fast_len = 10;
    int16_t slow_len = 30;
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

    virtual SIGNAL on_data(const Tin& input) override {
        tc.update(this->resolve_ts(input));

        fast_sma.update_buffer(input);
        slow_sma.update_buffer(input);

        // check we have data valid ma's
        if (!slow_sma[-1] || !slow_sma[-2]) {
            return SIGNAL::FLAT;
        }

        // LONG 
        if (this->bconfig.long_active) {
            if (fast_sma[-1] >= slow_sma[-1] && fast_sma[-2] < slow_sma[-2]) {
                return SIGNAL::LONG;
            }
        }

        // SHORT
        if (this->bconfig.short_active) {
            if (fast_sma[-1] <= slow_sma[-1] && fast_sma[-2] > slow_sma[-2]) {
                return SIGNAL::SHORT;
            }
        }


        return SIGNAL::FLAT;
    }

private:
    MACConfig params;

    SMA<double, double> fast_sma;
    SMA<double, double> slow_sma;

    TimeConverter tc = {};
    
};