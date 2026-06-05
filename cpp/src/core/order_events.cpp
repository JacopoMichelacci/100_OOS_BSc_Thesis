
#include "core/order_events.hpp"


// ORDER STATUS MEMBER
std::ostream& operator<<(std::ostream& os, ORDER_STATUS status) {
    switch (status) {
        case (ORDER_STATUS::PENDING): {return os << "pending";}
        case (ORDER_STATUS::FILLED): {return os << "filled";}
        case (ORDER_STATUS::PFILLED): {return os << "pfilled";}
        case (ORDER_STATUS::REJECTED): {return os << "rejected";}
        case (ORDER_STATUS::CANCELED): {return os << "canceled";}
        case (ORDER_STATUS::NONE): {return os << "none";}
    }

    return os;
}

// ORDER SIDE MEMBER
std::ostream& operator<<(std::ostream& os, ORDER_SIDE side) {
    switch (side) {
        case (ORDER_SIDE::BUY): {return os << "buy";}
        case (ORDER_SIDE::SELL): {return os << "sell";}
        case (ORDER_SIDE::NONE): {return os << "none";}
    }

    return os;
}

// ORDER EVENT
std::ostream& operator<<(std::ostream& os, const OrderEvent& ev) {
    os << "OrderEvent{"
       << "id="          << ev.id
       << ", ts="        << ev.ts
       << ", symbol="    << ev.symbol
       << ", side="      << ev.side
       << ", qty="       << ev.qty
       << ", price="     << ev.price
       << ", status="    << ev.status
       << ", strat_id="  << ev.strategy_id;
    
    if (!ev.reason.empty()) {
        os << ", reason=\"" << ev.reason << "\"";
    }
    
    os << "} | ";
    return os;
}