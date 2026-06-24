// cpp/backtest_main.cpp

#include <iostream>
#include <string>

#include "utils/csv_loader.hpp"
#include "utils/data_window.hpp"
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
    const std::string start_date = "";
    const std::string end_date = "";
    const double oos_split_pct = 0.25;

    auto all_data = load_csv<OHLCVEvent>(data_path);
    const bool use_date_split = !start_date.empty() || !end_date.empty();
    auto data = use_date_split
        ? filter_by_date(all_data, start_date, end_date)
        : split_by_pct(all_data, oos_split_pct, DATA_SPLIT_SIDE::LAST);

    
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
        "output/backtest/_assets",
        start_date,
        end_date,
        oos_split_pct,
        first_date(data),
        last_date(data)
    );
    write_backtest_results(data, results, "output/backtest/_assets");

    timer.end();
    timer.print("backtest runtime: ");

    return 0;
}
