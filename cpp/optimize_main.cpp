// cpp/optimize_main.cpp

#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "utils/csv_loader.hpp"
#include "utils/timer.hpp"
#include "core/market_events.hpp"
#include "strategies/built_in/std_mrev.hpp"
#include "backtest/backtest.hpp"


struct OptMetrics {
    double net_profit = 0.0;
    double avg_trade = 0.0;
    double sharpe = 0.0;
    double max_dd = 0.0;
    double n_trades_per_year = 0.0;
    int n_trades = 0;
};


struct OpenLot {
    double qty = 0.0;
    double entry_price = 0.0;
};


std::vector<int> arange(int start, int stop, int step) {
    std::vector<int> values;
    for (int value = start; value < stop; value += step) {
        values.push_back(value);
    }
    return values;
}


std::vector<double> arange(double start, double stop, double step) {
    std::vector<double> values;
    for (double value = start; value < stop; value += step) {
        values.push_back(value);
    }
    return values;
}


double calc_sharpe(const std::vector<double>& equity_curve, double periods_per_year = 252.0) {
    if (equity_curve.size() < 3) {
        return 0.0;
    }

    std::vector<double> returns;
    returns.reserve(equity_curve.size() - 1);
    for (std::size_t i = 1; i < equity_curve.size(); ++i) {
        if (equity_curve[i - 1] == 0.0) {
            continue;
        }
        returns.push_back(equity_curve[i] / equity_curve[i - 1] - 1.0);
    }

    if (returns.size() < 2) {
        return 0.0;
    }

    double sum = 0.0;
    for (double ret : returns) {
        sum += ret;
    }
    const double mean = sum / returns.size();

    double var = 0.0;
    for (double ret : returns) {
        const double diff = ret - mean;
        var += diff * diff;
    }
    var /= static_cast<double>(returns.size() - 1);

    if (var <= 0.0) {
        return 0.0;
    }

    return mean / std::sqrt(var) * std::sqrt(periods_per_year);
}


double calc_max_dd(const std::vector<double>& equity_curve) {
    if (equity_curve.empty()) {
        return 0.0;
    }

    double peak = equity_curve.front();
    double max_dd = 0.0;

    for (double equity : equity_curve) {
        if (equity > peak) {
            peak = equity;
        }
        if (peak != 0.0) {
            const double dd = equity / peak - 1.0;
            if (dd < max_dd) {
                max_dd = dd;
            }
        }
    }

    return max_dd * 100.0;
}


OptMetrics calc_metrics(const BacktestResults& results, double initial_capital, double periods_per_year = 252.0) {
    OptMetrics metrics;

    if (!results.equity_curve.empty()) {
        metrics.net_profit = results.equity_curve.back() - initial_capital;
    }
    metrics.sharpe = calc_sharpe(results.equity_curve, periods_per_year);
    metrics.max_dd = calc_max_dd(results.equity_curve);

    std::vector<OpenLot> open_lots;
    double total_trade_pnl = 0.0;

    for (const auto& order : results.hlog) {
        if (order.status != ORDER_STATUS::FILLED || order.qty == 0.0) {
            continue;
        }

        double signed_qty = 0.0;
        switch (order.signal) {
            case SIGNAL::BBUY:
            case SIGNAL::LONG:
            case SIGNAL::COVER:
                signed_qty = order.qty;
                break;
            case SIGNAL::BSELL:
            case SIGNAL::SHORT:
            case SIGNAL::SELL:
                signed_qty = -order.qty;
                break;
            default:
                continue;
        }

        double remaining_qty = signed_qty;

        while (!open_lots.empty() && open_lots.front().qty * remaining_qty < 0.0) {
            auto& lot = open_lots.front();
            const double close_qty = std::min(std::abs(lot.qty), std::abs(remaining_qty));
            const bool is_long = lot.qty > 0.0;
            const double pnl = is_long
                ? (order.price - lot.entry_price) * close_qty
                : (lot.entry_price - order.price) * close_qty;

            total_trade_pnl += pnl;
            ++metrics.n_trades;

            if (std::abs(lot.qty) == close_qty) {
                remaining_qty += lot.qty;
                open_lots.erase(open_lots.begin());
            }
            else {
                lot.qty += close_qty * (lot.qty > 0.0 ? -1.0 : 1.0);
                remaining_qty = 0.0;
            }
        }

        if (remaining_qty != 0.0) {
            open_lots.push_back(OpenLot{
                .qty = remaining_qty,
                .entry_price = order.price
            });
        }
    }

    metrics.avg_trade = metrics.n_trades ? total_trade_pnl / metrics.n_trades : 0.0;
    const double years = results.equity_curve.empty() ? 0.0 : results.equity_curve.size() / periods_per_year;
    metrics.n_trades_per_year = years > 0.0 ? metrics.n_trades / years : 0.0;
    return metrics;
}


int main() {
    Timer timer(TIMER_TYPE::MILLISECONDS);
    timer.start();
    const auto opt_start = std::chrono::steady_clock::now();

    const std::string data_path = "data/_data/equity/AAPL_ohlcv_2000-01-01_yf.csv";
    auto data = load_csv<OHLCVEvent>(data_path);

    BacktestConfig bt_config{
        .initial_capital = 100'000.0,
        .cost_bps = 0.0,
        .currency = "USD"
    };

    const int std_len = 90;
    const std::vector<double> thresholds = arange(0.5, 10.0, 0.1);
    const int iterations = static_cast<int>(thresholds.size());

    const std::string out_dir = "output/optimization/_assets";
    std::filesystem::create_directories(out_dir);

    std::ofstream out(out_dir + "/optimization_results.csv");
    if (!out) {
        std::cerr << "could not open optimization output csv\n";
        return 1;
    }

    out << "threshold,net_profit,avg_trade,sharpe,max_dd,n_trades,n_trades_per_year\n";

    double best_sharpe = -std::numeric_limits<double>::infinity();
    double best_threshold = 0.0;

    for (double threshold : thresholds) {
        Std_MRev<OHLCVEvent> strat("std_mrev_opt", Std_MrevConfig<OHLCVEvent>{
            .std_len = std_len,
            .lower_std_thresh = -threshold,
            .upper_std_thresh = threshold,
            .pos_sizing_mode = SIZING_MODE::FIXED_FRACTIONAL_PRICE,
            .qty = 1.0,
            .equity_pct = 0.01
        });

        Backtester<OHLCVEvent> bt(bt_config);
        const auto results = bt.run(strat, data);
        const auto metrics = calc_metrics(results, bt_config.initial_capital);

        out << threshold << ","
            << metrics.net_profit << ","
            << metrics.avg_trade << ","
            << metrics.sharpe << ","
            << metrics.max_dd << ","
            << metrics.n_trades << ","
            << metrics.n_trades_per_year << "\n";

        if (metrics.sharpe > best_sharpe) {
            best_sharpe = metrics.sharpe;
            best_threshold = threshold;
        }
    }

    std::ofstream metadata(out_dir + "/metadata.csv");
    metadata << "key,value\n";
    metadata << "strategy,Std_MRev\n";
    metadata << "data," << std::filesystem::path(data_path).stem().string() << "\n";
    metadata << "optimized_param,threshold\n";
    metadata << "fixed_std_len," << std_len << "\n";
    metadata << "objective,sharpe\n";
    metadata << "best_threshold," << best_threshold << "\n";
    metadata << "best_sharpe," << best_sharpe << "\n";

    const auto opt_end = std::chrono::steady_clock::now();
    const double opt_runtime_ms = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(opt_end - opt_start).count()
    ) / 1000.0;
    const double avg_runtime_ms = iterations ? opt_runtime_ms / iterations : 0.0;

    std::cout << "\noptimization complete"
              << ", std_len: " << std_len
              << ", best threshold: " << best_threshold
              << ", best sharpe: " << best_sharpe << "\n\n";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "iterations: " << iterations << "\n";
    std::cout << "optimization runtime: " << opt_runtime_ms << "ms\n";
    std::cout << "avg runtime per run: " << avg_runtime_ms << "ms\n";

    timer.end();

    return 0;
}
