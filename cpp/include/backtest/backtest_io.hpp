#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

#include "backtest/backtest.hpp"
#include "utils/time.hpp"

inline void write_backtest_metadata(
    std::string_view strat_name,
    const std::string& data_path,
    const std::string& out_dir = "output/backtest/_assets"
) {
    // Keep run metadata beside the generated data assets.
    std::filesystem::create_directories(out_dir);

    const auto metadata_path = out_dir + "/metadata.csv";
    std::ofstream metadata_out(metadata_path);
    if (!metadata_out) {
        std::cerr << "write_backtest_metadata: could not open " << metadata_path << "\n";
        return;
    }

    const auto data_name = std::filesystem::path(data_path).stem().string();

    metadata_out << "key,value\n";
    metadata_out << "strat_name," << strat_name << "\n";
    metadata_out << "data," << data_name << "\n";
    metadata_out << "currency,$\n";
}

template <typename Tin>
void write_backtest_results(
    std::vector<Tin>& input,
    const BacktestResults& results,
    const std::string& out_dir = "output/backtest/_assets"
) {
    // Ensure the fixed results directory exists before writing files.
    std::filesystem::create_directories(out_dir);

    const auto equity_path = out_dir + "/equity.csv";
    const auto orders_path = out_dir + "/orders.csv";

    // Write one equity value per input event timestamp.
    std::ofstream eq_out(equity_path);
    if (!eq_out) {
        std::cerr << "write_backtest_results: could not open " << equity_path << "\n";
        return;
    }

    eq_out << "ts,equity\n";
    const auto n = std::min(input.size(), results.equity_curve.size());
    for (std::size_t i = 0; i < n; ++i) {
        eq_out << get_timestamp(input[i]) << "," << results.equity_curve[i] << "\n";
    }

    if (input.size() != results.equity_curve.size()) {
        std::cerr << "write_backtest_results: input/equity size mismatch: "
                  << input.size() << " vs " << results.equity_curve.size() << "\n";
    }

    // Write all processed order events in execution/log order.
    std::ofstream orders_out(orders_path);
    if (!orders_out) {
        std::cerr << "write_backtest_results: could not open " << orders_path << "\n";
        return;
    }

    orders_out << "id,ts,signal,qty,price,status,reason\n";
    for (const auto& o : results.hlog) {
        orders_out << o.id << "," << o.ts << "," << o.signal << ","
                   << o.qty << "," << o.price << "," << o.status << ","
                   << o.reason << "\n";
    }
}
