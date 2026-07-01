// cpp/synthetic_main.cpp

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "utils/csv_loader.hpp"
#include "utils/timer.hpp"
#include "core/market_events.hpp"
#include "core/synthetic_market_generator.hpp"


void write_real_csv(const std::vector<OHLCVEvent>& data, const std::string& out_path) {
    std::filesystem::create_directories(std::filesystem::path(out_path).parent_path());

    std::ofstream out(out_path);
    if (!out) {
        throw std::runtime_error("could not open real path csv");
    }

    out << "ts,open,high,low,close,volume\n";
    for (const auto& row : data) {
        out << row.ts << ","
            << row.open << ","
            << row.high << ","
            << row.low << ","
            << row.close << ","
            << row.volume << "\n";
    }
}


int main() {
    Timer timer(TIMER_TYPE::MILLISECONDS);
    timer.start();

    const std::string data_path = "data/_data/equity/AAPL_ohlcv_2000-01-01_yf.csv";
    const std::string out_dir = "output/synthetic_compare/_assets";
    const int iter = 50;
    const int vol_window = 30;
    const double threshold_mult = 10.0;
    const int seed = 42;

    auto data = load_csv<OHLCVEvent>(data_path);

    SyntheticMarketGenerator<OHLCVEvent> generator(data);
    const auto gbm_paths = generator.gen_gbm(iter, seed);
    const auto rolling_vol_paths = generator.gen_gbm_rolling_vol(iter, vol_window, seed);
    const auto threshold_paths = generator.gen_gbm_rolling_vol_threshold(iter, vol_window, threshold_mult, seed, 10000);

    write_real_csv(data, out_dir + "/real_path.csv");
    generator.write_paths_csv(gbm_paths, out_dir + "/synthetic_paths_gbm.csv");
    generator.write_paths_csv(rolling_vol_paths, out_dir + "/synthetic_paths_gbm_rolling_vol.csv");
    generator.write_paths_csv(threshold_paths, out_dir + "/synthetic_paths_gbm_rolling_vol_threshold.csv");

    std::ofstream metadata(out_dir + "/metadata.csv");
    metadata << "key,value\n";
    metadata << "data," << std::filesystem::path(data_path).stem().string() << "\n";
    metadata << "generators,gen_gbm|gen_gbm_rolling_vol|gen_gbm_rolling_vol_threshold\n";
    metadata << "iterations," << iter << "\n";
    metadata << "vol_window," << vol_window << "\n";
    metadata << "threshold_mult," << threshold_mult << "\n";
    metadata << "seed," << seed << "\n";

    std::cout << "\ngenerated synthetic paths"
              << ", generators: 3"
              << ", iterations: " << iter
              << ", rows per path: " << data.size() << "\n\n";

    timer.end();
    timer.print("synthetic generation runtime: ");

    return 0;
}
