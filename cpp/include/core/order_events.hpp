#pragma once

#include <ostream>
#include <string>
#include <utility>

/*
 * SIGNAL semantics:
 *
 * Strict (rejected if state mismatch):
 *   LONG   → open long position    (valid when open_position >= 0)
 *   SHORT  → open short position   (valid when open_position <= 0)
 *   SELL   → close long position   (valid when open_position > 0)
 *   COVER  → close short position  (valid when open_position < 0)
 *
 * Blind (state-blind, just transact qty in this direction):
 *   BBUY   → buy  qty: position += qty  (scales in, covers shorts, flips if qty > |pos|)
 *   BSELL  → sell qty: position -= qty
 *
 *   FLAT   → no-op
 *
 * Invalid strict signals are logged as REJECTED orders. Blind signals never internally reject based on direction reasons.
 */
enum class SIGNAL : std::int8_t {
    SELL = -2,
    SHORT = -1,
    FLAT = 0,
    LONG = 1,
    COVER = 2,

    BBUY = 9,
    BSELL = -9,
};
std::ostream& operator<<(std::ostream& os, SIGNAL sig);

enum class ORDER_STATUS : std::int8_t {
    PENDING = 0,
    FILLED = 1,
    PFILLED = 2,
    REJECTED = -1,
    CANCELED = -9
};
std::ostream& operator<<(std::ostream& os, ORDER_STATUS status);

enum class ORDER_TYPE : std::int8_t {
    MARKET = 1,
    NONE = 0
};
std::ostream& operator<<(std::ostream& os, ORDER_TYPE type);

enum class STOP_TYPE : std::int8_t {
    NONE = 0,
    HARD = 1,
};
std::ostream& operator<<(std::ostream& os, STOP_TYPE type);

struct StopLoss {
    STOP_TYPE type = STOP_TYPE::NONE;
    double slnot = -1.0;
    double slpct = -1.0;  // percent, e.g. 2.0 = 2%; negative disables
};

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
    StopLoss sl = {};
    long long strategy_id = -1;                 // a unique identifier for a strategy; unique int given at construction 
    long long id = -1;                          // id of the order event
    long long pid = -1;                         // parent entry order id; -1 for entries
    ORDER_STATUS status = ORDER_STATUS::PENDING;
    std::string reason = "";
};
std::ostream& operator<<(std::ostream& os, const OrderEvent& e);
