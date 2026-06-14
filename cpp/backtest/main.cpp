// cpp/backtest/main.cpp

#include <iostream>
#include <string>

#include "utils/csv_loader.hpp"
#include "core/market_events.hpp"


#include "strategies/built_in/ma_cross.hpp"
#include "backtest/backtest.hpp"
#include "backtest/backtest_io.hpp"


int main() {
    // 1. load data
    auto data = load_csv<OHLCVEvent>("data/_data/equity/AAPL_ohlcv_2000-01-01_yf.csv");

    
    // 2. set up strategy
    MAC<OHLCVEvent> strat("mac_demo", MACConfig<OHLCVEvent>{
        .fast_len = 10,
        .slow_len = 30,
        .qty = 1.0
    });

    // 3. set up backtester
    Backtester<OHLCVEvent> bt(BacktestConfig{
        .initial_capital = 100'000.0,
        .cost_bps = 3.0
    });

    // 4. run
    auto results = bt.run(strat, data);
    std::cout << "ran backtest, final equity: " << results.equity_curve.back() 
              << ", n orders: " << results.hlog.size() << "\n";

    // Export raw results for Python reporting/plotting.
    write_backtest_results(data, results, "output/backtest/_assets");

    return 0;
}
