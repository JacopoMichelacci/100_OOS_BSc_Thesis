// cpp/optimize_main.cpp

#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "utils/csv_loader.hpp"
#include "utils/data_window.hpp"
#include "utils/timer.hpp"
#include "core/market_events.hpp"
#include "core/synthetic_market_generator.hpp"
#include "strategies/built_in/ma_cross.hpp"
#include "strategies/built_in/std_mrev.hpp"
#include "backtest/backtest.hpp"


struct OptMetrics {
    double net_profit = 0.0;
    double avg_trade = 0.0;
    double sharpe = 0.0;
    double max_dd = 0.0;
    double max_dd_not = 0.0;
    double n_trades_per_year = 0.0;
    double n_trades = 0.0;
};


struct OpenLot {
    double qty = 0.0;
    double entry_price = 0.0;
};


std::vector<double> arange(int start, int stop, int step) {
    std::vector<double> values;
    for (int value = start; value < stop; value += step) {
        values.push_back(static_cast<double>(value));
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


enum class OPT_STRATEGY {
    MAC,
    STD_MREV
};


enum class OPT_PARAM {
    NONE,
    MAC_FAST_LEN,
    MAC_SLOW_LEN,
    MAC_SLPCT,
    STD_LEN,
    STD_THRESH,
    STD_SLPCT
};


enum class OPT_DATA_MODE {
    HISTORICAL_IS,
    SYNTHETIC_IS
};


struct ExperimentConfig {
    // main experiment controls
    OPT_STRATEGY strategy = OPT_STRATEGY::MAC;
    OPT_PARAM optimized_param_1 = OPT_PARAM::MAC_SLPCT;
    OPT_PARAM optimized_param_2 = OPT_PARAM::NONE;
    std::vector<double> grid_1 = arange(0.0, 30.0, 0.1);
    std::vector<double> grid_2 = {};
    OPT_DATA_MODE data_mode = OPT_DATA_MODE::HISTORICAL_IS;

    // data controls
    std::string data_path = "data/_data/equity/AAPL_ohlcv_2000-01-01_yf.csv";
    std::string start_date = "";
    std::string end_date = "";
    double oos_test_pct = 0.30;

    // backtest controls
    double initial_capital = 100'000.0;
    double cost_bps = 0.0;
    std::string currency = "USD";

    // synthetic controls
    int synthetic_iter = 50;
    int synthetic_vol_window = 30;
    double synthetic_threshold_mult = 10.0;
    int synthetic_seed = 42;
    int synthetic_max_attempts = 10000;

    // MAC resting params
    int mac_fast_len = 50;
    int mac_slow_len = 100;
    PRICE_FIELD mac_fast_price_field = PRICE_FIELD::CLOSE;
    PRICE_FIELD mac_slow_price_field = PRICE_FIELD::CLOSE;
    double mac_slpct = -1.0;
    double mac_slnot = -1.0;

    // Std_MRev resting params
    int std_len = 15;
    PRICE_FIELD std_price_field = PRICE_FIELD::CLOSE;
    double std_lower_thresh = -1.5;
    double std_upper_thresh = 1.5;
    double std_slpct = -1.0;
    double std_slnot = -1.0;
};


const char* strategy_name(OPT_STRATEGY strategy) {
    switch (strategy) {
        case OPT_STRATEGY::MAC:
            return "MAC";
        case OPT_STRATEGY::STD_MREV:
            return "Std_MRev";
        default:
            return "UNKNOWN";
    }
}


const char* param_name(OPT_PARAM param) {
    switch (param) {
        case OPT_PARAM::NONE:
            return "none";
        case OPT_PARAM::MAC_FAST_LEN:
            return "fast_len";
        case OPT_PARAM::MAC_SLOW_LEN:
            return "slow_len";
        case OPT_PARAM::MAC_SLPCT:
            return "slpct";
        case OPT_PARAM::STD_LEN:
            return "std_len";
        case OPT_PARAM::STD_THRESH:
            return "threshold";
        case OPT_PARAM::STD_SLPCT:
            return "slpct";
        default:
            return "unknown_param";
    }
}


const char* data_mode_name(OPT_DATA_MODE mode) {
    switch (mode) {
        case OPT_DATA_MODE::HISTORICAL_IS:
            return "historical_is";
        case OPT_DATA_MODE::SYNTHETIC_IS:
            return "synthetic_is";
        default:
            return "unknown_data_mode";
    }
}


BacktestResults run_one_backtest(
    const ExperimentConfig& cfg,
    double param_value_1,
    double param_value_2,
    const std::vector<OHLCVEvent>& data,
    const BacktestConfig& bt_config
) {
    Backtester<OHLCVEvent> bt(bt_config);

    if (cfg.strategy == OPT_STRATEGY::MAC) {
        int fast_len = cfg.mac_fast_len;
        int slow_len = cfg.mac_slow_len;
        double slpct = cfg.mac_slpct;

        auto apply_param = [&](OPT_PARAM param, double value) {
            switch (param) {
                case OPT_PARAM::NONE:
                    break;
                case OPT_PARAM::MAC_FAST_LEN:
                    fast_len = static_cast<int>(std::lround(value));
                    break;
                case OPT_PARAM::MAC_SLOW_LEN:
                    slow_len = static_cast<int>(std::lround(value));
                    break;
                case OPT_PARAM::MAC_SLPCT:
                    slpct = value / 100.0;
                    break;
                default:
                    throw std::invalid_argument("selected optimized_param is not valid for MAC");
            }
        };

        apply_param(cfg.optimized_param_1, param_value_1);
        apply_param(cfg.optimized_param_2, param_value_2);

        MAC<OHLCVEvent> strat("mac_opt", MACConfig<OHLCVEvent>{
            .fast_len = static_cast<int16_t>(fast_len),
            .fast_price_field = cfg.mac_fast_price_field,
            .slow_len = static_cast<int16_t>(slow_len),
            .slow_price_field = cfg.mac_slow_price_field,
            .slnot = cfg.mac_slnot,
            .slpct = slpct,
            .pos_sizing_mode = SIZING_MODE::FIXED,
            .qty = 1.0
        });

        return bt.run(strat, data);
    }

    if (cfg.strategy == OPT_STRATEGY::STD_MREV) {
        int std_len = cfg.std_len;
        double lower_thresh = cfg.std_lower_thresh;
        double upper_thresh = cfg.std_upper_thresh;
        double slpct = cfg.std_slpct;

        auto apply_param = [&](OPT_PARAM param, double value) {
            switch (param) {
                case OPT_PARAM::NONE:
                    break;
                case OPT_PARAM::STD_LEN:
                    std_len = static_cast<int>(std::lround(value));
                    break;
                case OPT_PARAM::STD_THRESH:
                    lower_thresh = -value;
                    upper_thresh = value;
                    break;
                case OPT_PARAM::STD_SLPCT:
                    slpct = value / 100.0;
                    break;
                default:
                    throw std::invalid_argument("selected optimized_param is not valid for Std_MRev");
            }
        };

        apply_param(cfg.optimized_param_1, param_value_1);
        apply_param(cfg.optimized_param_2, param_value_2);

        Std_MRev<OHLCVEvent> strat("std_mrev_opt", Std_MrevConfig<OHLCVEvent>{
            .std_len = std_len,
            .price_field = cfg.std_price_field,
            .lower_std_thresh = lower_thresh,
            .upper_std_thresh = upper_thresh,
            .slnot = cfg.std_slnot,
            .slpct = slpct,
            .pos_sizing_mode = SIZING_MODE::FIXED,
            .qty = 1.0
        });

        return bt.run(strat, data);
    }

    throw std::invalid_argument("unknown optimization strategy");
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


double calc_max_dd_not(const std::vector<double>& equity_curve) {
    if (equity_curve.empty()) {
        return 0.0;
    }

    double peak = equity_curve.front();
    double max_dd = 0.0;

    for (double equity : equity_curve) {
        if (equity > peak) {
            peak = equity;
        }
        const double dd = equity - peak;
        if (dd < max_dd) {
            max_dd = dd;
        }
    }

    return max_dd;
}


OptMetrics calc_metrics(const BacktestResults& results, double initial_capital, double periods_per_year = 252.0) {
    OptMetrics metrics;

    if (!results.equity_curve.empty()) {
        metrics.net_profit = results.equity_curve.back() - initial_capital;
    }
    metrics.sharpe = calc_sharpe(results.equity_curve, periods_per_year);
    metrics.max_dd = calc_max_dd(results.equity_curve);
    metrics.max_dd_not = calc_max_dd_not(results.equity_curve);

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


OptMetrics calc_mean_metrics(
    const ExperimentConfig& cfg,
    double param_value_1,
    double param_value_2,
    const std::vector<OHLCVEvent>& historical_data,
    const std::vector<std::vector<OHLCVEvent>>& synthetic_paths,
    const BacktestConfig& bt_config
) {
    if (cfg.data_mode == OPT_DATA_MODE::HISTORICAL_IS) {
        const auto results = run_one_backtest(cfg, param_value_1, param_value_2, historical_data, bt_config);
        return calc_metrics(results, bt_config.initial_capital);
    }

    if (cfg.data_mode != OPT_DATA_MODE::SYNTHETIC_IS) {
        throw std::invalid_argument("unknown optimization data mode");
    }
    if (synthetic_paths.empty()) {
        throw std::invalid_argument("synthetic optimization requires at least one synthetic path");
    }

    OptMetrics mean_metrics;

    for (const auto& path : synthetic_paths) {
        const auto results = run_one_backtest(cfg, param_value_1, param_value_2, path, bt_config);
        const auto metrics = calc_metrics(results, bt_config.initial_capital);

        mean_metrics.net_profit += metrics.net_profit;
        mean_metrics.avg_trade += metrics.avg_trade;
        mean_metrics.sharpe += metrics.sharpe;
        mean_metrics.max_dd += metrics.max_dd;
        mean_metrics.max_dd_not += metrics.max_dd_not;
        mean_metrics.n_trades += metrics.n_trades;
        mean_metrics.n_trades_per_year += metrics.n_trades_per_year;
    }

    const double n_paths = static_cast<double>(synthetic_paths.size());
    mean_metrics.net_profit /= n_paths;
    mean_metrics.avg_trade /= n_paths;
    mean_metrics.sharpe /= n_paths;
    mean_metrics.max_dd /= n_paths;
    mean_metrics.max_dd_not /= n_paths;
    mean_metrics.n_trades /= n_paths;
    mean_metrics.n_trades_per_year /= n_paths;

    return mean_metrics;
}


int main() {
    Timer timer(TIMER_TYPE::MICROSECONDS);
    timer.start();

    // Change this block for normal optimization experiments.
    ExperimentConfig cfg{
        .strategy = OPT_STRATEGY::STD_MREV,
        .optimized_param_1 = OPT_PARAM::STD_THRESH,
        .optimized_param_2 = OPT_PARAM::NONE,
        .grid_1 = arange(0.5, 5.0, 0.1),
        .grid_2 = {},
        .data_mode = OPT_DATA_MODE::HISTORICAL_IS,

        .data_path = "data/_data/equity/AAPL_ohlcv_2000-01-01_yf.csv",
        .start_date = "",
        .end_date = "",
        .oos_test_pct = 0.30,

        .initial_capital = 100'000.0,
        .cost_bps = 0.0,
        .currency = "USD",

        .synthetic_iter = 50,
        .synthetic_vol_window = 30,
        .synthetic_threshold_mult = 10.0,
        .synthetic_seed = 42,
        .synthetic_max_attempts = 10000,

        .mac_fast_len = 50,
        .mac_slow_len = 100,
        .mac_fast_price_field = PRICE_FIELD::CLOSE,
        .mac_slow_price_field = PRICE_FIELD::CLOSE,
        .mac_slpct = -1.0,
        .mac_slnot = -1.0,

        .std_len = 15,
        .std_price_field = PRICE_FIELD::CLOSE,
        .std_lower_thresh = -1.5,
        .std_upper_thresh = 1.5,
        .std_slpct = -1.0,
        .std_slnot = -1.0
    };

    const double is_optimization_pct = 1.0 - cfg.oos_test_pct;

    auto all_data = load_csv<OHLCVEvent>(cfg.data_path);
    // if dates are chosen date split is overthrown
    const bool use_date_split = !cfg.start_date.empty() || !cfg.end_date.empty();
    auto data = use_date_split
        ? filter_by_date(all_data, cfg.start_date, cfg.end_date)
        : split_by_pct(all_data, is_optimization_pct, DATA_SPLIT_SIDE::FIRST);

    BacktestConfig bt_config{
        .initial_capital = cfg.initial_capital,
        .cost_bps = cfg.cost_bps,
        .currency = cfg.currency
    };

    const bool has_second_param = cfg.optimized_param_2 != OPT_PARAM::NONE;
    const int iterations = static_cast<int>(
        cfg.grid_1.size() * (has_second_param ? cfg.grid_2.size() : 1)
    );
    const std::string optimized_param_name_1 = param_name(cfg.optimized_param_1);
    const std::string optimized_param_name_2 = param_name(cfg.optimized_param_2);
    const int estimated_backtests = iterations * (
        cfg.data_mode == OPT_DATA_MODE::SYNTHETIC_IS ? cfg.synthetic_iter : 1
    );
    const double estimated_runtime_sec = estimated_backtests * 0.001;
    const double estimated_runtime_min = estimated_runtime_sec / 60.0;

    std::cout << "\noptimization plan\n";
    std::cout << "strategy: " << strategy_name(cfg.strategy) << "\n";
    std::cout << "data mode: " << data_mode_name(cfg.data_mode) << "\n";
    std::cout << "param 1: " << optimized_param_name_1
              << ", grid size: " << cfg.grid_1.size() << "\n";
    if (has_second_param) {
        std::cout << "param 2: " << optimized_param_name_2
                  << ", grid size: " << cfg.grid_2.size() << "\n";
    }
    std::cout << "planned parameter combinations: " << iterations << "\n";
    std::cout << "estimated underlying backtests: " << estimated_backtests << "\n";
    std::cout << "rough runtime estimate @ 1ms/backtest: "
              << estimated_runtime_sec << "s"
              << " (" << estimated_runtime_min << "min)\n\n";

    std::vector<std::vector<OHLCVEvent>> synthetic_paths;
    if (cfg.data_mode == OPT_DATA_MODE::SYNTHETIC_IS) {
        SyntheticMarketGenerator<OHLCVEvent> generator(data);
        synthetic_paths = generator.gen_gbm_rolling_vol_threshold(
            cfg.synthetic_iter,
            cfg.synthetic_vol_window,
            cfg.synthetic_threshold_mult,
            cfg.synthetic_seed,
            cfg.synthetic_max_attempts
        );
    }

    const std::string out_dir = "output/optimization/_assets";
    std::filesystem::create_directories(out_dir);

    std::ofstream out(out_dir + "/optimization_results.csv");
    if (!out) {
        std::cerr << "could not open optimization output csv\n";
        return 1;
    }

    out << optimized_param_name_1;
    if (has_second_param) {
        out << "," << optimized_param_name_2;
    }
    out << ",net_profit,avg_trade,sharpe,max_dd,max_dd_not,n_trades,n_trades_per_year\n";

    double best_sharpe = -std::numeric_limits<double>::infinity();
    double best_value_1 = 0.0;
    double best_value_2 = 0.0;
    int evaluated_iterations = 0;

    for (double param_value_1 : cfg.grid_1) {
        const auto& grid_2 = has_second_param ? cfg.grid_2 : std::vector<double>{0.0};

        for (double param_value_2 : grid_2) {
            try {
                const auto metrics = calc_mean_metrics(
                    cfg,
                    param_value_1,
                    param_value_2,
                    data,
                    synthetic_paths,
                    bt_config
                );

                out << param_value_1;
                if (has_second_param) {
                    out << "," << param_value_2;
                }
                out << ","
                    << metrics.net_profit << ","
                    << metrics.avg_trade << ","
                    << metrics.sharpe << ","
                    << metrics.max_dd << ","
                    << metrics.max_dd_not << ","
                    << metrics.n_trades << ","
                    << metrics.n_trades_per_year << "\n";

                ++evaluated_iterations;

                if (metrics.sharpe > best_sharpe) {
                    best_sharpe = metrics.sharpe;
                    best_value_1 = param_value_1;
                    best_value_2 = param_value_2;
                }
            }
            catch (const std::invalid_argument&) {
                continue;
            }
        }
    }

    std::ofstream metadata(out_dir + "/metadata.csv");
    metadata << "key,value\n";
    metadata << "strategy," << strategy_name(cfg.strategy) << "\n";
    metadata << "data_mode," << data_mode_name(cfg.data_mode) << "\n";
    metadata << "data," << std::filesystem::path(cfg.data_path).stem().string() << "\n";
    metadata << "start_date," << cfg.start_date << "\n";
    metadata << "end_date," << cfg.end_date << "\n";
    metadata << "is_start_date," << first_date(data) << "\n";
    metadata << "is_end_date," << last_date(data) << "\n";
    metadata << "is_optimization_pct," << is_optimization_pct << "\n";
    metadata << "oos_test_pct," << cfg.oos_test_pct << "\n";
    metadata << "optimized_param," << optimized_param_name_1 << "\n";
    metadata << "optimized_param_1," << optimized_param_name_1 << "\n";
    metadata << "optimized_param_2," << optimized_param_name_2 << "\n";
    metadata << "mac_fast_len," << cfg.mac_fast_len << "\n";
    metadata << "mac_slow_len," << cfg.mac_slow_len << "\n";
    metadata << "mac_slpct," << cfg.mac_slpct << "\n";
    metadata << "std_len," << cfg.std_len << "\n";
    metadata << "std_lower_thresh," << cfg.std_lower_thresh << "\n";
    metadata << "std_upper_thresh," << cfg.std_upper_thresh << "\n";
    metadata << "std_slpct," << cfg.std_slpct << "\n";
    metadata << "synthetic_iter," << cfg.synthetic_iter << "\n";
    metadata << "synthetic_vol_window," << cfg.synthetic_vol_window << "\n";
    metadata << "synthetic_threshold_mult," << cfg.synthetic_threshold_mult << "\n";
    metadata << "synthetic_seed," << cfg.synthetic_seed << "\n";
    metadata << "objective,sharpe\n";
    metadata << "best_" << optimized_param_name_1 << "," << best_value_1 << "\n";
    metadata << "best_" << optimized_param_name_2 << "," << best_value_2 << "\n";
    metadata << "best_value_1," << best_value_1 << "\n";
    metadata << "best_value_2," << best_value_2 << "\n";
    metadata << "best_sharpe," << best_sharpe << "\n";
    metadata << "planned_iterations," << iterations << "\n";
    metadata << "evaluated_iterations," << evaluated_iterations << "\n";

    // timer
    const double opt_runtime_ms = static_cast<double>(timer.end()) / 1000.0;
    const int backtests_per_iteration = cfg.data_mode == OPT_DATA_MODE::SYNTHETIC_IS ? cfg.synthetic_iter : 1;
    const int evaluated_backtests = evaluated_iterations * backtests_per_iteration;
    const double avg_runtime_ms = evaluated_backtests != 0 ? opt_runtime_ms / evaluated_backtests : 0.0;

    std::cout << "\noptimization complete"
              << ", strategy: " << strategy_name(cfg.strategy)
              << ", data mode: " << data_mode_name(cfg.data_mode)
              << ", optimized param 1: " << optimized_param_name_1
              << ", best value 1: " << best_value_1;
    if (has_second_param) {
        std::cout << ", optimized param 2: " << optimized_param_name_2
                  << ", best value 2: " << best_value_2;
    }
    std::cout
              << ", best sharpe: " << best_sharpe << "\n\n";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "evaluated iterations: " << evaluated_iterations << "\n";
    std::cout << "evaluated backtests: " << evaluated_backtests << "\n";
    std::cout << "optimization runtime: " << opt_runtime_ms << "ms\n";
    std::cout << "avg runtime per backtest: " << avg_runtime_ms << "ms\n";

    return 0;
}
