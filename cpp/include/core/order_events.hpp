#pragma once
#include <ostream>
#include <string>
#include <utility>


enum class ORDER_STATUS {
    PENDING,
    FILLED,
    PFILLED,
    REJECTED,
    CANCELED,
    NONE
};
std::ostream& operator<<(std::ostream& os, ORDER_STATUS status);

enum class ORDER_SIDE {
    BUY,
    SELL,
    NONE
};
std::ostream& operator<<(std::ostream& os, ORDER_SIDE side);

class OrderIdGenerator {
public:
    long long next() {
        return ++id_;
    }

private:
    long long id_ = 0;
};


class OrderEvent {
public:
    long long ts = 0;
    ORDER_SIDE side = ORDER_SIDE::NONE;
    std::string symbol = "";
    double qty = 0.0;
    double price = 0.0;
    long long strategy_id = -1;                 // a unique identifier for a strategy; unique int given at construction 
    long long id = -1;                          // id of the order event
    ORDER_STATUS status = ORDER_STATUS::NONE;
};