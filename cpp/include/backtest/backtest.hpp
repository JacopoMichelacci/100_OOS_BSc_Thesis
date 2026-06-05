#pragma once

#include <vector>

#include "core/order_events.hpp"
#include "strategies/strategy_base.hpp"



struct BacktestResults {
    std::vector<double> equity_curve;
    std::vector<OrderEvent> hlog;
};

struct BacktestConfig {
    double initial_capital = 100'000.0;
    double cost_bps = 3.0;
};


template <typename Tin>
class Backtester {
public:
    Backtester(Strategy<Tin> strat_, BacktestConfig cfg_) 
        : strat(strat_), cfg(cfg_) {}

private:
    Strategy<Tin>& strat;
    BacktestConfig cfg;
    BacktestResults results;
};