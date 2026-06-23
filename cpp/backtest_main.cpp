// cpp/backtest_main.cpp

#include <iostream>
#include <string>

#include "utils/csv_loader.hpp"
#include "utils/timer.hpp"
#include "core/market_events.hpp"


#include "strategies/built_in/std_mrev.hpp"
#include "backtest/backtest.hpp"
#include "backtest/backtest_io.hpp"


int main() {
    Timer timer(TIMER_TYPE::MILLISECONDS);
    timer.start();

    // 1. load data
    const std::string data_path = "data/_data/equity/AAPL_ohlcv_2000-01-01_yf.csv";
    auto data = load_csv<OHLCVEvent>(data_path);

    
    // 2. set up strategy
    Std_MRev<OHLCVEvent> strat("std_mrev_demo", Std_MrevConfig<OHLCVEvent>{
        .std_len = 15,
        .lower_std_thresh = -2.5,
        .upper_std_thresh = 2.5,
        .qty = 1.0
    });

    // 3. set up backtester
    BacktestConfig bt_config{
        .initial_capital = 100'000.0,
        .cost_bps = 0.0,
        .currency = "USD"
    };
    Backtester<OHLCVEvent> bt(bt_config);

    // 4. run
    auto results = bt.run(strat, data);
    std::cout << "\nran backtest, final equity: " << results.equity_curve.back() 
              << ", n orders: " << results.hlog.size() << "\n\n";

    // Export raw results for Python reporting/plotting.
    write_backtest_metadata(
        strat.get_name(),
        data_path,
        bt_config.cost_bps,
        bt_config.currency,
        "output/backtest/_assets"
    );
    write_backtest_results(data, results, "output/backtest/_assets");

    timer.end();
    timer.print("backtest runtime: ");

    return 0;
}
