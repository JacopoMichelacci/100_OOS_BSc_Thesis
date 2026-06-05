#include "strategies/strategy_base.hpp"


// SIGNAL OVERLOAD
std::ostream& operator<<(std::ostream& os, SIGNAL sig) {
    switch (sig) {
        case SIGNAL::SELL:  os << "sell";  break;
        case SIGNAL::SHORT: os << "short"; break;
        case SIGNAL::FLAT:  os << "flat";  break;
        case SIGNAL::LONG:  os << "long";  break;
        case SIGNAL::COVER: os << "cover"; break;

        case SIGNAL::BBUY:  os << "bbuy";  break;
        case SIGNAL::BSELL: os << "bsell"; break;
    }
    return os;
}