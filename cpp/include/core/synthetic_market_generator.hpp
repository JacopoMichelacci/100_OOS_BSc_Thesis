#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>


template <typename Tin>
class SyntheticMarketGenerator {
public:
    explicit SyntheticMarketGenerator(std::vector<Tin> data_)
        : data(std::move(data_)) {
        if (data.empty()) {
            throw std::invalid_argument("SyntheticMarketGenerator requires non-empty data");
        }
    }

    const std::vector<Tin>& get_data() const {
        return data;
    }

    void write_paths_csv(
        const std::vector<std::vector<Tin>>& paths,
        const std::string& out_path
    ) const {
        std::filesystem::create_directories(std::filesystem::path(out_path).parent_path());

        std::ofstream out(out_path);
        if (!out) {
            throw std::runtime_error("write_paths_csv could not open output path");
        }

        out << "path_id,ts,open,high,low,close,volume\n";
        for (std::size_t path_id = 0; path_id < paths.size(); ++path_id) {
            for (const auto& row : paths[path_id]) {
                out << path_id << ","
                    << row.ts << ","
                    << row.open << ","
                    << row.high << ","
                    << row.low << ","
                    << row.close << ","
                    << row.volume << "\n";
            }
        }
    }

    // basic GBM with one global drift and volatility estimate
    std::vector<std::vector<Tin>> gen_gbm(int iter, int seed = 42) const {
        // safety checks
        if (iter < 1) {
            throw std::invalid_argument("gen_gbm iter must be positive");
        }
        if (data.size() < 2) {
            throw std::invalid_argument("gen_gbm requires at least 2 data points");
        }

        // compute historical close-to-close log returns
        std::vector<double> log_returns;
        std::vector<double> high_exts;
        std::vector<double> low_exts;
        log_returns.reserve(data.size() - 1);
        high_exts.reserve(data.size() - 1);
        low_exts.reserve(data.size() - 1);

        for (std::size_t i = 1; i < data.size(); ++i) {
            if (data[i - 1].close <= 0.0 || data[i].close <= 0.0) {
                throw std::invalid_argument("gen_gbm requires positive close prices");
            }
            if (data[i].open <= 0.0 || data[i].high <= 0.0 || data[i].low <= 0.0) {
                throw std::invalid_argument("gen_gbm requires positive OHLC prices");
            }

            log_returns.push_back(std::log(data[i].close / data[i - 1].close));

            const double bar_high_base = std::max(data[i].open, data[i].close);
            const double bar_low_base = std::min(data[i].open, data[i].close);
            high_exts.push_back(std::log(std::max(data[i].high, bar_high_base) / bar_high_base));
            low_exts.push_back(std::log(bar_low_base / std::min(data[i].low, bar_low_base)));
        }

        // estimate per-bar drift
        double sum = 0.0;
        for (double ret : log_returns) {
            sum += ret;
        }
        const double mu = sum / log_returns.size();

        // estimate per-bar volatility
        double var = 0.0;
        for (double ret : log_returns) {
            const double diff = ret - mu;
            var += diff * diff;
        }
        var /= static_cast<double>(log_returns.size() - 1);
        const double sigma = std::sqrt(var);

        // estimate global high / low wick behavior
        const auto high_stats = mean_std(high_exts);
        const auto low_stats = mean_std(low_exts);

        std::mt19937 rng(seed);
        std::normal_distribution<double> norm(0.0, 1.0);

        // store synthetic paths
        std::vector<std::vector<Tin>> paths;
        paths.reserve(iter);

        for (int path_idx = 0; path_idx < iter; ++path_idx) {
            // copy timestamps / metadata, overwrite prices
            std::vector<Tin> path = data;
            path[0].open = data[0].open;
            path[0].high = data[0].high;
            path[0].low = data[0].low;
            path[0].close = data[0].close;

            for (std::size_t i = 1; i < path.size(); ++i) {
                // mu and sigma are per input bar, so dt is implicitly 1 bar
                const double ret = (mu - 0.5 * sigma * sigma) + sigma * norm(rng);
                const double close = prev_close * std::exp(ret);
                const double open = prev_close;
                const double high_ext = std::max(0.0, high_stats.mean + high_stats.std * norm(rng));
                const double low_ext = std::max(0.0, low_stats.mean + low_stats.std * norm(rng));

                path[i].open = open;
                path[i].close = close;
                apply_high_low(path[i], open, close, high_ext, low_ext);

                prev_close = close;
            }

            paths.emplace_back(std::move(path));
        }

        return paths;
    }

    // GBM with time-varying volatility estimated from a rolling return window
    std::vector<std::vector<Tin>> gen_gbm_rolling_vol(int iter, int vol_window = 5, int seed = 42) const {
        // safety checks
        if (iter < 1) {
            throw std::invalid_argument("gen_gbm_rolling_vol iter must be positive");
        }
        if (vol_window < 2) {
            throw std::invalid_argument("gen_gbm_rolling_vol vol_window must be at least 2");
        }
        if (data.size() < 2) {
            throw std::invalid_argument("gen_gbm_rolling_vol requires at least 2 data points");
        }

        // compute historical close-to-close log returns
        std::vector<double> log_returns;
        std::vector<double> high_exts;
        std::vector<double> low_exts;
        log_returns.reserve(data.size() - 1);
        high_exts.reserve(data.size() - 1);
        low_exts.reserve(data.size() - 1);

        for (std::size_t i = 1; i < data.size(); ++i) {
            if (data[i - 1].close <= 0.0 || data[i].close <= 0.0) {
                continue;
            }
            if (data[i].open <= 0.0 || data[i].high <= 0.0 || data[i].low <= 0.0) {
                continue;
            }

            log_returns.push_back(std::log(data[i].close / data[i - 1].close));

            const double bar_high_base = std::max(data[i].open, data[i].close);
            const double bar_low_base = std::min(data[i].open, data[i].close);
            high_exts.push_back(std::log(std::max(data[i].high, bar_high_base) / bar_high_base));
            low_exts.push_back(std::log(bar_low_base / std::min(data[i].low, bar_low_base)));
        }

        if (log_returns.size() < 2) {
            throw std::invalid_argument("gen_gbm_rolling_vol requires at least 2 valid close returns");
        }

        // estimate global per-bar drift
        double sum = 0.0;
        for (double ret : log_returns) {
            sum += ret;
        }
        const double mu = sum / log_returns.size();

        // estimate rolling per-bar volatility and high / low wick behavior
        const auto rolling_ret_stats = rolling_mean_std(log_returns, vol_window);
        const auto rolling_high_stats = rolling_mean_std(high_exts, vol_window);
        const auto rolling_low_stats = rolling_mean_std(low_exts, vol_window);

        std::mt19937 rng(seed);
        std::normal_distribution<double> norm(0.0, 1.0);

        std::vector<std::vector<Tin>> paths;
        paths.reserve(iter);

        for (int path_idx = 0; path_idx < iter; ++path_idx) {
            // copy timestamps / metadata, overwrite prices
            std::vector<Tin> path = data;
            double prev_close = data[0].close;
            path[0].open = prev_close;
            path[0].high = prev_close;
            path[0].low = prev_close;
            path[0].close = prev_close;

            for (std::size_t i = 1; i < path.size(); ++i) {
                const double sigma = rolling_ret_stats[i - 1].std;
                // mu and sigma are per input bar, so dt is implicitly 1 bar
                const double ret = (mu - 0.5 * sigma * sigma) + sigma * norm(rng);
                const double close = prev_close * std::exp(ret);
                const double open = prev_close;
                const double high_ext = std::max(0.0, rolling_high_stats[i - 1].mean + rolling_high_stats[i - 1].std * norm(rng));
                const double low_ext = std::max(0.0, rolling_low_stats[i - 1].mean + rolling_low_stats[i - 1].std * norm(rng));

                path[i].open = open;
                path[i].close = close;
                apply_high_low(path[i], open, close, high_ext, low_ext);

                prev_close = close;
            }

            paths.emplace_back(std::move(path));
        }

        return paths;
    }

    // rolling-vol GBM constrained inside a local threshold band from the original series
    std::vector<std::vector<Tin>> gen_gbm_rolling_vol_threshold(
        int iter,
        int vol_window = 5,
        double threshold_mult = 2.0,
        int seed = 42,
        int max_attempts = 1000
    ) const {
        // safety checks
        if (iter < 1) {
            throw std::invalid_argument("gen_gbm_rolling_vol_threshold iter must be positive");
        }
        if (vol_window < 2) {
            throw std::invalid_argument("gen_gbm_rolling_vol_threshold vol_window must be at least 2");
        }
        if (threshold_mult <= 0.0) {
            throw std::invalid_argument("gen_gbm_rolling_vol_threshold threshold_mult must be positive");
        }
        if (max_attempts < 1) {
            throw std::invalid_argument("gen_gbm_rolling_vol_threshold max_attempts must be positive");
        }
        if (data.size() < 2) {
            throw std::invalid_argument("gen_gbm_rolling_vol_threshold requires at least 2 data points");
        }

        // compute historical close-to-close log returns
        std::vector<double> log_returns;
        std::vector<double> high_exts;
        std::vector<double> low_exts;
        log_returns.reserve(data.size() - 1);
        high_exts.reserve(data.size() - 1);
        low_exts.reserve(data.size() - 1);
        
        for (std::size_t i = 1; i < data.size(); ++i) {
            if (data[i - 1].close <= 0.0 || data[i].close <= 0.0) {
                throw std::invalid_argument("gen_gbm_rolling_vol_threshold requires positive close prices");
            }
            if (data[i].open <= 0.0 || data[i].high <= 0.0 || data[i].low <= 0.0) {
                throw std::invalid_argument("gen_gbm_rolling_vol_threshold requires positive OHLC prices");
            }

            log_returns.push_back(std::log(data[i].close / data[i - 1].close));

            const double bar_high_base = std::max(data[i].open, data[i].close);
            const double bar_low_base = std::min(data[i].open, data[i].close);
            high_exts.push_back(std::log(std::max(data[i].high, bar_high_base) / bar_high_base));
            low_exts.push_back(std::log(bar_low_base / std::min(data[i].low, bar_low_base)));
        }

        if (log_returns.size() < 2) {
            throw std::invalid_argument("gen_gbm_rolling_vol_threshold requires at least 2 valid close returns");
        }

        // estimate global per-bar drift
        double sum = 0.0;
        for (double ret : log_returns) {
            sum += ret;
        }
        const double mu = sum / log_returns.size();

        // estimate rolling per-bar volatility and high / low wick behavior
        const auto rolling_ret_stats = rolling_mean_std(log_returns, vol_window);
        const auto rolling_high_stats = rolling_mean_std(high_exts, vol_window);
        const auto rolling_low_stats = rolling_mean_std(low_exts, vol_window);

        std::mt19937 rng(seed);
        std::normal_distribution<double> norm(0.0, 1.0);

        std::vector<std::vector<Tin>> paths;
        paths.reserve(iter);

        for (int path_idx = 0; path_idx < iter; ++path_idx) {
            // copy timestamps / metadata, overwrite prices
            std::vector<Tin> path = data;
            double prev_close = data[0].close;
            path[0].open = prev_close;
            path[0].high = prev_close;
            path[0].low = prev_close;
            path[0].close = prev_close;

            for (std::size_t i = 1; i < path.size(); ++i) {
                const double sigma = rolling_ret_stats[i - 1].std;
                // threshold band is anchored on the original local price level
                const double original_anchor = data[i - 1].close;
                const double lower_bound = original_anchor * std::exp(-threshold_mult * sigma);
                const double upper_bound = original_anchor * std::exp(threshold_mult * sigma);

                double close = prev_close;
                bool accepted = false;

                for (int attempt = 0; attempt < max_attempts; ++attempt) {
                    // resample until the next synthetic close stays inside the band
                    const double ret = (mu - 0.5 * sigma * sigma) + sigma * norm(rng);
                    close = prev_close * std::exp(ret);

                    if (close >= lower_bound && close <= upper_bound) {
                        accepted = true;
                        break;
                    }
                }

                if (!accepted) {
                    close = std::clamp(close, lower_bound, upper_bound);
                }

                const double open = prev_close;
                const double high_ext = std::max(0.0, rolling_high_stats[i - 1].mean + rolling_high_stats[i - 1].std * norm(rng));
                const double low_ext = std::max(0.0, rolling_low_stats[i - 1].mean + rolling_low_stats[i - 1].std * norm(rng));

                path[i].open = open;
                path[i].close = close;
                apply_high_low(path[i], open, close, high_ext, low_ext);

                prev_close = close;
            }

            paths.emplace_back(std::move(path));
        }

        return paths;
    }

private:
    struct MeanStd {
        double mean = 0.0;
        double std = 0.0;
    };

    static MeanStd mean_std(const std::vector<double>& values) {
        if (values.empty()) {
            return {};
        }

        double sum = 0.0;
        for (double value : values) {
            sum += value;
        }
        const double mean = sum / values.size();

        double var = 0.0;
        for (double value : values) {
            const double diff = value - mean;
            var += diff * diff;
        }
        var /= values.size() > 1 ? static_cast<double>(values.size() - 1) : 1.0;

        return {.mean = mean, .std = std::sqrt(var)};
    }

    static std::vector<MeanStd> rolling_mean_std(const std::vector<double>& values, int window) {
        std::vector<MeanStd> out;
        out.reserve(values.size());

        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto start = i + 1 > static_cast<std::size_t>(window)
                ? i + 1 - static_cast<std::size_t>(window) : 0;
            std::vector<double> slice;
            slice.reserve(i + 1 - start);

            for (std::size_t j = start; j <= i; ++j) {
                slice.push_back(values[j]);
            }

            out.push_back(mean_std(slice));
        }

        return out;
    }

    static void apply_high_low(Tin& row, double open, double close, double high_ext, double low_ext) {
        const double high_base = std::max(open, close);
        const double low_base = std::min(open, close);

        row.high = high_base * std::exp(high_ext);
        row.low = low_base * std::exp(-low_ext);
    }

    std::vector<Tin> data;
};
