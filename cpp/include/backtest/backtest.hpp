#pragma once

#include <string>
#include <vector>
#include <span>
#include <algorithm>

#include "core/order_events.hpp"
#include "strategies/strategy_base.hpp"



struct BacktestResults {
    std::vector<double> equity_curve;
    std::vector<OrderEvent> hlog;
};

struct BacktestConfig {
    double initial_capital = 100'000.0;
    double cost_bps = 3.0;
    std::string currency = "USD";
};

template <typename Tin>
class Backtester {
public:
    explicit Backtester(BacktestConfig cfg_ = {}) 
        : cfg(cfg_) {}


    BacktestResults run(Strategy<Tin>& strat, std::span<const Tin> input) {
        // reset state
        results = {};
        results.equity_curve.reserve(input.size());

        double cash = cfg.initial_capital;
        double opos = 0.0;
        std::vector<OrderEvent> oposlog;
        OrderIdGenerator id_gen;
        std::vector<OrderEvent> plog;
        plog.reserve(4);
        results.hlog.reserve(128);

        // main loop
        for (std::size_t i = 0; i < input.size(); ++i) {
            const auto& in = input[i];

            // execute pending orders (no look ahead)
            execute_orders(plog, in.open, in.ts, cash, opos, oposlog, id_gen, strat);

            // hlog update
            results.hlog.insert(results.hlog.end(), plog.begin(), plog.end());

            // call on data
            plog.clear();
            StrategyContext ctx{
                .opos = opos,
                .equity = cash + opos * in.close
            };
            strat.on_data(in, ctx, plog);

            // mtm update
            results.equity_curve.emplace_back(cash + opos * in.close);
        }

        return results;
    }


private:
    BacktestConfig cfg;
    BacktestResults results;


    void reject(OrderEvent& order, const std::string& reason) {
        order.status = ORDER_STATUS::REJECTED;
        order.reason = reason;
    }

    void fill_buy(OrderEvent& order, double exec_price, double& cash, double& opos) {
        double cost = exec_price * order.qty * (cfg.cost_bps / 10000.0);
        cash -= exec_price * order.qty + cost;
        opos += order.qty;
        order.status = ORDER_STATUS::FILLED;
    }

    void fill_sell(OrderEvent& order, double exec_price, double& cash, double& opos) {
        double cost = exec_price * order.qty * (cfg.cost_bps / 10000.0);
        cash += exec_price * order.qty - cost;
        opos -= order.qty;
        order.status = ORDER_STATUS::FILLED;
    }

    void add_open_lot(std::vector<OrderEvent>& oposlog,
                      const OrderEvent& order,
                      SIGNAL side,
                      double qty) {
        if (qty <= 0.0) return;

        OrderEvent lot = order;
        lot.signal = side;
        lot.qty = qty;
        lot.status = ORDER_STATUS::FILLED;
        lot.reason = "";
        oposlog.emplace_back(lot);
    }

    void close_open_lots(std::vector<OrderEvent>& oposlog,
                         SIGNAL side,
                         double qty) {
        double remaining = qty;
        
        // traverse oposlog and reduce quantity with FIFO
        for (auto it = oposlog.begin(); it != oposlog.end() && remaining > 0.0;) {
            if (it->signal != side) {
                ++it;
                continue;
            }

            const double closed_qty = std::min(it->qty, remaining);
            it->qty -= closed_qty;
            remaining -= closed_qty;

            if (it->qty <= 0.0) {
                it = oposlog.erase(it);
            }
            else {
                ++it;
            }
        }
    }


    void execute_orders(std::vector<OrderEvent>& orders,
                        double exec_price,
                        long long exec_ts,
                        double& cash,
                        double& opos,
                        std::vector<OrderEvent>& oposlog,
                        OrderIdGenerator& id_gen,
                        Strategy<Tin>& strat) {

        const auto& sc = strat.get_config();

        for (auto& ord : orders) {
            // SIGNAL::FLAT skipped (cleared on next plog.clear() anyway)
            if (ord.signal == SIGNAL::FLAT) continue;

            ord.price       = exec_price;
            ord.ts          = exec_ts;
            ord.strategy_id = strat.get_id();
            ord.id          = id_gen.next();

            if (!sc.active) { reject(ord, "strategy not active"); continue; }

            switch (ord.signal) {
                case SIGNAL::LONG:
                    if (!sc.long_active)              { reject(ord, "longs disabled"); break; }
                    if (opos < 0)                     { reject(ord, "LONG while short"); break; }
                    if (!sc.stacking && opos > 0)     { reject(ord, "stacking disabled"); break; }
                    fill_buy(ord, exec_price, cash, opos);
                    add_open_lot(oposlog, ord, SIGNAL::LONG, ord.qty);
                    break;

                case SIGNAL::SHORT:
                    if (!sc.short_active)             { reject(ord, "shorts disabled"); break; }
                    if (opos > 0)                     { reject(ord, "SHORT while long"); break; }
                    if (!sc.stacking && opos < 0)     { reject(ord, "stacking disabled"); break; }
                    fill_sell(ord, exec_price, cash, opos);
                    add_open_lot(oposlog, ord, SIGNAL::SHORT, ord.qty);
                    break;

                case SIGNAL::SELL:
                    if (opos <= 0)                    { reject(ord, "SELL while not long"); break; }
                    if (ord.qty > opos)               { reject(ord, "SELL exceeds long position"); break; }
                    fill_sell(ord, exec_price, cash, opos);
                    close_open_lots(oposlog, SIGNAL::LONG, ord.qty);
                    break;

                case SIGNAL::COVER:
                    if (opos >= 0)                    { reject(ord, "COVER while not short"); break; }
                    if (ord.qty > -opos)              { reject(ord, "COVER exceeds short position"); break; }
                    fill_buy(ord, exec_price, cash, opos);
                    close_open_lots(oposlog, SIGNAL::SHORT, ord.qty);
                    break;

                case SIGNAL::BBUY:
                    if (!sc.long_active && (opos >= 0 || ord.qty > -opos)) {
                        reject(ord, "longs disabled"); break;
                    }
                    if (!sc.stacking && opos > 0)     { reject(ord, "stacking disabled"); break; }
                    {
                    const double prev_opos = opos;
                    fill_buy(ord, exec_price, cash, opos);

                    // have meaningful and signal independent oposlog
                    if (prev_opos < 0.0) {
                        close_open_lots(oposlog, SIGNAL::SHORT, std::min(ord.qty, -prev_opos));
                    }
                    if (prev_opos >= 0.0) {
                        add_open_lot(oposlog, ord, SIGNAL::LONG, ord.qty);
                    }
                    else if (opos > 0.0) {
                        add_open_lot(oposlog, ord, SIGNAL::LONG, opos);
                    }
                    }
                    break;

                case SIGNAL::BSELL:
                    if (!sc.short_active && (opos <= 0 || ord.qty > opos)) {
                        reject(ord, "shorts disabled"); break;
                    }
                    if (!sc.stacking && opos < 0)     { reject(ord, "stacking disabled"); break; }
                    {
                    const double prev_opos = opos;
                    fill_sell(ord, exec_price, cash, opos);
                    if (prev_opos > 0.0) {
                        close_open_lots(oposlog, SIGNAL::LONG, std::min(ord.qty, prev_opos));
                    }
                    if (prev_opos <= 0.0) {
                        add_open_lot(oposlog, ord, SIGNAL::SHORT, ord.qty);
                    }
                    else if (opos < 0.0) {
                        add_open_lot(oposlog, ord, SIGNAL::SHORT, -opos);
                    }
                    }
                    break;

                default: break;
            }
        }
    }
};
