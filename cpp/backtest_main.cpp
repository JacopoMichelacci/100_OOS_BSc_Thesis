// cpp/backtest_main.cpp

#include <cmath>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "utils/csv_loader.hpp"
#include "utils/data_window.hpp"
#include "utils/timer.hpp"
#include "core/market_events.hpp"
#include "core/synthetic_market_generator.hpp"


#include "strategies/built_in/ma_cross.hpp"
#include "backtest/backtest.hpp"
#include "backtest/backtest_io.hpp"

enum class BT_SAMPLE {
    IS,
    OOS,
    ALL
};


enum class THESIS_EVAL_MODE {
    STANDARD,
    SYNTHETIC,
    BOTH
};


const char* sample_name(BT_SAMPLE sample) {
    switch (sample) {
        case BT_SAMPLE::IS:
            return "IS";
        case BT_SAMPLE::OOS:
            return "OOS";
        case BT_SAMPLE::ALL:
            return "ALL";
        default:
            return "UNKNOWN";
    }
}


struct ThesisMetrics {
    double net_profit = 0.0;
    double max_dd_pct = 0.0;
    double sharpe = 0.0;
    double avg_profit = 0.0;
    double trades_per_year = 0.0;
    int n_trades = 0;
};


struct OpenLot {
    double qty = 0.0;
    double entry_price = 0.0;
};


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


double calc_max_dd_pct(const std::vector<double>& equity_curve) {
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


ThesisMetrics calc_thesis_metrics(
    const BacktestResults& results,
    double initial_capital,
    double periods_per_year = 252.0
) {
    ThesisMetrics metrics;

    if (!results.equity_curve.empty()) {
        metrics.net_profit = results.equity_curve.back() - initial_capital;
    }
    metrics.max_dd_pct = calc_max_dd_pct(results.equity_curve);
    metrics.sharpe = calc_sharpe(results.equity_curve, periods_per_year);

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

    metrics.avg_profit = metrics.n_trades ? total_trade_pnl / metrics.n_trades : 0.0;
    const double years = results.equity_curve.empty() ? 0.0 : results.equity_curve.size() / periods_per_year;
    metrics.trades_per_year = years > 0.0 ? metrics.n_trades / years : 0.0;

    return metrics;
}


void print_metrics_row(const std::string& label, const ThesisMetrics& metrics) {
    std::cout << std::left << std::setw(10) << label
              << std::right
              << std::setw(14) << metrics.net_profit
              << std::setw(14) << metrics.max_dd_pct
              << std::setw(14) << metrics.sharpe
              << std::setw(14) << metrics.avg_profit
              << std::setw(14) << metrics.trades_per_year
              << "\n";
}


ThesisMetrics mean_metrics(const std::vector<ThesisMetrics>& metrics_list) {
    ThesisMetrics out;
    if (metrics_list.empty()) {
        return out;
    }

    for (const auto& metrics : metrics_list) {
        out.net_profit += metrics.net_profit;
        out.max_dd_pct += metrics.max_dd_pct;
        out.sharpe += metrics.sharpe;
        out.avg_profit += metrics.avg_profit;
        out.trades_per_year += metrics.trades_per_year;
        out.n_trades += metrics.n_trades;
    }

    const double n = static_cast<double>(metrics_list.size());
    out.net_profit /= n;
    out.max_dd_pct /= n;
    out.sharpe /= n;
    out.avg_profit /= n;
    out.trades_per_year /= n;
    out.n_trades = static_cast<int>(out.n_trades / metrics_list.size());

    return out;
}


ThesisMetrics diff_metrics(const ThesisMetrics& is_metrics, const ThesisMetrics& oos_metrics) {
    return ThesisMetrics{
        .net_profit = oos_metrics.net_profit - is_metrics.net_profit,
        .max_dd_pct = oos_metrics.max_dd_pct - is_metrics.max_dd_pct,
        .sharpe = oos_metrics.sharpe - is_metrics.sharpe,
        .avg_profit = oos_metrics.avg_profit - is_metrics.avg_profit,
        .trades_per_year = oos_metrics.trades_per_year - is_metrics.trades_per_year,
        .n_trades = oos_metrics.n_trades - is_metrics.n_trades
    };
}


void print_metrics_table(
    const std::string& title,
    const ThesisMetrics& is_metrics,
    const ThesisMetrics& oos_metrics
) {
    std::cout << "\n" << title << " (Diff = OOS - IS)\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::left << std::setw(10) << "Sample"
              << std::right
              << std::setw(14) << "Net Profit"
              << std::setw(14) << "Max DD %"
              << std::setw(14) << "Sharpe"
              << std::setw(14) << "Avg Profit"
              << std::setw(14) << "Trades/Year"
              << "\n";
    print_metrics_row("IS", is_metrics);
    print_metrics_row("OOS", oos_metrics);
    print_metrics_row("Diff", diff_metrics(is_metrics, oos_metrics));
    std::cout << "\n";
}


MAC<OHLCVEvent> make_strategy();


void run_standard_thesis_eval(
    const std::vector<OHLCVEvent>& is_data,
    const std::vector<OHLCVEvent>& oos_data,
    const BacktestConfig& bt_config
) {
    auto is_strat = make_strategy();
    Backtester<OHLCVEvent> is_bt(bt_config);
    const auto is_results = is_bt.run(is_strat, is_data);

    auto oos_strat = make_strategy();
    Backtester<OHLCVEvent> oos_bt(bt_config);
    const auto oos_results = oos_bt.run(oos_strat, oos_data);

    const auto is_metrics = calc_thesis_metrics(is_results, bt_config.initial_capital);
    const auto oos_metrics = calc_thesis_metrics(oos_results, bt_config.initial_capital);

    print_metrics_table("standard thesis summary", is_metrics, oos_metrics);
}


void run_synthetic_thesis_eval(
    const std::vector<OHLCVEvent>& is_data,
    const std::vector<OHLCVEvent>& oos_data,
    const BacktestConfig& bt_config,
    int synthetic_iter,
    int synthetic_vol_window,
    double synthetic_threshold_mult
) {
    auto oos_strat = make_strategy();
    Backtester<OHLCVEvent> oos_bt(bt_config);
    const auto oos_results = oos_bt.run(oos_strat, oos_data);
    const auto oos_metrics = calc_thesis_metrics(oos_results, bt_config.initial_capital);

    SyntheticMarketGenerator<OHLCVEvent> generator(is_data);
    const auto synthetic_paths = generator.gen_gbm_rolling_vol_threshold(
        synthetic_iter,
        synthetic_vol_window,
        synthetic_threshold_mult
    );

    std::vector<ThesisMetrics> synthetic_metrics;
    synthetic_metrics.reserve(synthetic_paths.size());

    for (const auto& path : synthetic_paths) {
        auto syn_strat = make_strategy();
        Backtester<OHLCVEvent> syn_bt(bt_config);
        const auto syn_results = syn_bt.run(syn_strat, path);
        synthetic_metrics.push_back(calc_thesis_metrics(syn_results, bt_config.initial_capital));
    }

    const auto synthetic_is_metrics = mean_metrics(synthetic_metrics);
    print_metrics_table("synthetic thesis summary", synthetic_is_metrics, oos_metrics);
}


MAC<OHLCVEvent> make_strategy() {
    return MAC<OHLCVEvent>("ma_cross_demo", MACConfig<OHLCVEvent>{
        .fast_len = 35,
        .fast_price_field = PRICE_FIELD::CLOSE,
        .slow_len = 200,
        .slow_price_field = PRICE_FIELD::CLOSE,
        .slnot = -1.0,
        .slpct = -1.0,
        .pos_sizing_mode = SIZING_MODE::FIXED,
        .qty = 1.0
    });
}


int main() {
    Timer timer(TIMER_TYPE::MILLISECONDS);
    timer.start();

    // 1. load data
    const std::string data_path = "data/_data/equity/AAPL_ohlcv_2000-01-01_yf.csv";
    const std::string start_date = "";
    const std::string end_date = "";
    const double oos_test_pct = 0.3;
    const double is_optimization_pct = 1.0 - oos_test_pct;
    const BT_SAMPLE sample = BT_SAMPLE::ALL;
    const THESIS_EVAL_MODE thesis_eval_mode = THESIS_EVAL_MODE::BOTH;
    const int synthetic_iter = 50;
    const int synthetic_vol_window = 30;
    const double synthetic_threshold_mult = 10.0;

    auto all_data = load_csv<OHLCVEvent>(data_path);
    const bool use_date_split = !start_date.empty() || !end_date.empty();
    const auto split_source = use_date_split
        ? filter_by_date(all_data, start_date, end_date)
        : all_data;
    std::vector<OHLCVEvent> data;

    if (use_date_split) {
        data = split_source;
    }
    else if (sample == BT_SAMPLE::IS) {
        data = split_by_pct(split_source, is_optimization_pct, DATA_SPLIT_SIDE::FIRST);
    }
    else if (sample == BT_SAMPLE::OOS) {
        data = split_by_pct(split_source, oos_test_pct, DATA_SPLIT_SIDE::LAST);
    }
    else {
        data = split_source;
    }

    // 2. set up backtester
    BacktestConfig bt_config{
        .initial_capital = 100'000.0,
        .cost_bps = 0.0,
        .currency = "USD"
    };

    // 3. thesis IS/OOS metrics
    const auto is_data = split_by_pct(split_source, is_optimization_pct, DATA_SPLIT_SIDE::FIRST);
    const auto oos_data = split_by_pct(split_source, oos_test_pct, DATA_SPLIT_SIDE::LAST);

    if (thesis_eval_mode == THESIS_EVAL_MODE::STANDARD || thesis_eval_mode == THESIS_EVAL_MODE::BOTH) {
        run_standard_thesis_eval(
            is_data,
            oos_data,
            bt_config
        );
    }

    if (thesis_eval_mode == THESIS_EVAL_MODE::SYNTHETIC || thesis_eval_mode == THESIS_EVAL_MODE::BOTH) {
        run_synthetic_thesis_eval(
            is_data,
            oos_data,
            bt_config,
            synthetic_iter,
            synthetic_vol_window,
            synthetic_threshold_mult
        );
    }

    // 4. run selected sample for the normal report
    auto strat = make_strategy();
    Backtester<OHLCVEvent> bt(bt_config);
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
        oos_test_pct,
        sample_name(sample),
        first_date(data),
        last_date(data)
    );
    write_backtest_results(data, results, "output/backtest/_assets");

    timer.end();
    timer.print("backtest runtime: ");

    return 0;
}
