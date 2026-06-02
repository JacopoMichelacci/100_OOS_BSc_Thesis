
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

// ORDER SIDE MEMBER
std::ostream& operator<<(std::ostream& os, ORDER_SIDE side) {
    switch (side) {
        case (ORDER_SIDE::BUY): {return os << "buy";}
        case (ORDER_SIDE::SELL): {return os << "sell";}
    }

    return os;
}