
#include "core/order_events.hpp"


// ORDER STATUS MEMBER
std::ostream& operator<<(std::ostream& os, ORDER_STATUS status) {
    switch (status) {
        case (ORDER_STATUS::PENDING): {return os << "pending";}
        case (ORDER_STATUS::FILLED): {return os << "filled";}
        case (ORDER_STATUS::PFILLED): {return os << "pfilled";}
        case (ORDER_STATUS::REJECTED): {return os << "rejected";}
        case (ORDER_STATUS::CANCELED): {return os << "canceled";}
    }

    return os;
}

// ORDER TYPE
std::ostream& operator<<(std::ostream& os, ORDER_TYPE type) {
    switch (type) {
        case (ORDER_TYPE::MARKET): {return os << "market";}
        case (ORDER_TYPE::NONE): {return os << "none";}
    }

    return os;
}


// ORDER EVENT
std::ostream& operator<<(std::ostream& os, const OrderEvent& ev) {
    os << "OrderEvent{"
       << "id="          << ev.id
       << ", ts="        << ev.ts
       << ", symbol="    << ev.symbol
       << ", signal="      << ev.signal
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