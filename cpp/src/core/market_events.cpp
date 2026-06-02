#include <iostream>

#include "core/market_events.hpp"

std::ostream& operator<<(std::ostream& os, MKT_EVENT_TYPE type) {
    switch (type) {
        case (MKT_EVENT_TYPE::OHLCV) : {return os << "ohlcv";}
        case (MKT_EVENT_TYPE::QUOTE) : {return os << "quote";}
        case (MKT_EVENT_TYPE::NONE) : {return os << "";}
    }

    return os;
}


///////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const OHLCVEvent& e) {
    os << "| ohlcv_event(type: " << e.type
       << ", open: " << e.open
       << ", high: " << e.high
       << ", low: " << e.low
       << ", close: " << e.close
       << ", volume: " << e.volume << ") |";
    return os;
}


std::ostream& operator<<(std::ostream& os, const QuoteEvent& e) {
    os << "| QuoteEvent(type: " << e.type
       << ", bid: " << e.bid
       << ", ask: " << e.ask
       << ", bidsize: " << e.bidsize
       << ", asksize: " << e.asksize << ") |";
    return os;
}

///////////////////////////////////////////////////////////////////////////