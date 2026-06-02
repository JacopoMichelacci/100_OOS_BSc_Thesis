#pragma once

#include <ostream>


enum class MKT_EVENT_TYPE {
    OHLCV,
    QUOTE,
    NONE
};

struct MarketEvent {
    long long ts = 0;
    MKT_EVENT_TYPE type = MKT_EVENT_TYPE::NONE;
};
std::ostream& operator<<(std::ostream& os, MKT_EVENT_TYPE type);



struct OHLCVEvent : public MarketEvent {
    MKT_EVENT_TYPE type = MKT_EVENT_TYPE::OHLCV;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
};
std::ostream& operator<<(std::ostream& os, const OHLCVEvent& e);


struct QuoteEvent : public MarketEvent {
    MKT_EVENT_TYPE type = MKT_EVENT_TYPE::QUOTE;
    double bid = 0.0;
    double ask = 0.0;
    double bidsize = 0.0;
    double asksize = 0.0;
};
std::ostream& operator<<(std::ostream& os, const QuoteEvent& e);
