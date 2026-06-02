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
};
std::ostream& operator<<(std::ostream& os, ORDER_STATUS status);

enum class ORDER_SIDE {
    BUY,
    SELL,
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
    long long ts;
    ORDER_SIDE side;
    std::string symbol;
    double qty;
    double price;
    long long strategy_id;            // a unique identifier for a strategy; unique int given at construction 
    long long id;
    ORDER_STATUS status;

    OrderEvent(long long ts_, ORDER_SIDE side_, std::string symbol_, double qty_, double price_, long long strategy_id_, long long id_, ORDER_STATUS status_) 
        : ts(ts_), side(side_), symbol(std::move(symbol_)), qty(qty_), price(price_), strategy_id(strategy_id_), id(id_), status(status_) {}
};