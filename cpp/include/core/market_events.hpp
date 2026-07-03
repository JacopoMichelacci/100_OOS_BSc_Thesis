#pragma once

#include <cstdint>
#include <exception>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "utils/time.hpp"

enum class MKT_EVENT_TYPE : std::uint8_t {
    OHLCV,
    QUOTE,
    NONE
};

struct MarketEvent {
    long long ts = 0;
    MKT_EVENT_TYPE type = MKT_EVENT_TYPE::NONE;

    virtual ~MarketEvent() = default;
};
std::ostream& operator<<(std::ostream& os, MKT_EVENT_TYPE type);



struct OHLCVEvent : public MarketEvent {
    OHLCVEvent() { type = MKT_EVENT_TYPE::OHLCV; }
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;


    static OHLCVEvent from_csv_row(const std::vector<std::string>& row) {
        OHLCVEvent ev;

        if (row.size() < 6) {
            std::cerr << "OHLCVEvent::from_csv_row: expected 6 columns, got "
                      << row.size() << "\n";
            return ev;
        }

        try {
            ev.ts = to_epoch_ms(row[0]);
        } catch (const std::exception& e) {
            std::cerr << "OHLCVEvent::from_csv_row: invalid timestamp \""
                      << row[0] << "\": " << e.what() << "\n";
        }

        auto parse_double = [&row](std::size_t idx, const char* name) {
            try {
                return std::stod(row[idx]);
            } catch (const std::exception& e) {
                std::cerr << "OHLCVEvent::from_csv_row: invalid " << name
                          << " \"" << row[idx] << "\": " << e.what() << "\n";
                return 0.0;
            }
        };

        ev.open = parse_double(1, "open");
        ev.high = parse_double(2, "high");
        ev.low = parse_double(3, "low");
        ev.close = parse_double(4, "close");
        ev.volume = parse_double(5, "volume");

        return ev;
    }
};
std::ostream& operator<<(std::ostream& os, const OHLCVEvent& e);


struct QuoteEvent : public MarketEvent {
    QuoteEvent() { type = MKT_EVENT_TYPE::QUOTE; }
    double bid = 0.0;
    double ask = 0.0;
    double bidsize = 0.0;
    double asksize = 0.0;
};
std::ostream& operator<<(std::ostream& os, const QuoteEvent& e);
