#pragma once

#include <ostream>
#include <string>
#include <utility>

#include "strategies/strategy_base.hpp"


enum class ORDER_STATUS {
    PENDING,
    FILLED,
    PFILLED,
    REJECTED,
    CANCELED,
};
std::ostream& operator<<(std::ostream& os, ORDER_STATUS status);

enum class ORDER_TYPE {
    MARKET,
    NONE
};
std::ostream& operator<<(std::ostream& os, ORDER_TYPE type);

class OrderIdGenerator {
public:
    long long next() {
        return ++id_;
    }

private:
    long long id_ = 0;
};


/*
 * OrderEvent represents both a strategy's intent and the backtester's execution log entry.
 *
 * Fields filled by strategy (on order send):
 *   - signal (SIGNAL)
 *   - qty    (double)
 *   - type   (ORDER_TYPE) — defaults to MARKET, set explicitly for LIMIT/STOP
 *
 * Fields filled by backtester (on order processing):
 *   - ts            (timestamp at execution)
 *   - symbol        (asset traded)
 *   - price         (execution price)
 *   - strategy_id   (which strategy issued it)
 *   - id            (unique order id)
 *   - status        (FILLED, REJECTED, etc.)
 *   - reason        (only if REJECTED)
 */
class OrderEvent {
public:
    long long ts = 0;
    SIGNAL signal = SIGNAL::FLAT;
    ORDER_TYPE type = ORDER_TYPE::NONE;
    std::string symbol = "";
    double qty = 0.0;
    double price = 0.0;
    long long strategy_id = -1;                 // a unique identifier for a strategy; unique int given at construction 
    long long id = -1;                          // id of the order event
    ORDER_STATUS status = ORDER_STATUS::PENDING;
    std::string reason = "";
};
std::ostream& operator<<(std::ostream& os, const OrderEvent& e);