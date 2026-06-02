#pragma once

#include <vector>
#include <string>

#include "strategies/strategy_base.hpp"
#include "indicators/built_in/moving_average.hpp"

struct MACConfig {
    int fast_len = 10;
    int slow_len = 30;
};

template <typename Tin>
class MAC : public Strategy<Tin> {
public:
    MAC(std::string name_, MACConfig& params_, StrategyConfig base_config_ = {}) 
        : Strategy<Tin>(detail::resolve_name(name_, __FILE__), base_config_),
        params(params_),

        fast_sma(params_.fast_len),
        slow_sma(params_.slow_len) {}

    virtual Signal on_data(const Tin& input) override {
        fast_sma.update_buffer(input);
        slow_sma.update_buffer(input);

        if (fast_sma.history.size() - 1 < params.fast_len) {
            return Signal::FLAT;
        }

        if (fast_sma[-1] > slow_sma[-1] )
    }

private:
    MACConfig params;
    SMA<double, double> fast_sma;
    SMA<double, double> slow_sma;
    
};