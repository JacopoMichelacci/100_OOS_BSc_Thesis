// cpp/backtest/main.cpp

#include <iostream>
#include <fstream>
#include <string>

#include "utils/csv_loader.hpp"
#include "core/market_events.hpp"


#include "strategies/built_in/ma_cross.hpp"
#include "backtest/backtest.hpp"


int main() {
    // 1. load data
    auto data = load_csv<OHLCVEvent>("data/_data/equity/AAPL_ohlcv_2000-01-01_yf.csv");
    std::cout << "loaded " << data.size() << " bars\n";
    
    // 2. set up strategy
    MAC<OHLCVEvent> strat("mac_demo", MACConfig<OHLCVEvent>{
        .fast_len = 10,
        .slow_len = 30,
        .qty = 1.0
    });

    // 3. set up backtester
    Backtester<OHLCVEvent> bt(strat, BacktestConfig{
        .initial_capital = 100'000.0,
        .cost_bps = 3.0
    });

    // 4. run
    auto results = bt.run(data);
    std::cout << "ran backtest, final equity: " << results.equity_curve.back() 
              << ", n orders: " << results.hlog.size() << "\n";

    // 5. dump equity curve
    std::ofstream eq_out("data/_data/equity.csv");
    eq_out << "ts,equity\n";
    for (std::size_t i = 0; i < results.equity_curve.size(); ++i) {
        eq_out << data[i].ts << "," << results.equity_curve[i] << "\n";
    }

    // 6. dump order log
    std::ofstream log_out("data/_data/hlog.csv");
    log_out << "id,ts,signal,qty,price,status,reason\n";
    for (const auto& o : results.hlog) {
        log_out << o.id << "," << o.ts << "," << o.signal << ","
                << o.qty << "," << o.price << "," << o.status << ","
                << o.reason << "\n";
    }

    return 0;
}
