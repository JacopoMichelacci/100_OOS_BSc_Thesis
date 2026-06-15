#pragma once

#include <string>
#include <vector>
#include <span>

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
        double eprice = 0.0;
        OrderIdGenerator id_gen;
        std::vector<OrderEvent> plog;
        plog.reserve(4);
        results.hlog.reserve(128);

        // main loop
        for (std::size_t i = 0; i < input.size(); ++i) {
            const auto& in = input[i];

            // execute pending orders (no look ahead)
            execute_orders(plog, in.open, in.ts, cash, opos, eprice, id_gen, strat);

            // hlog update
            results.hlog.insert(results.hlog.end(), plog.begin(), plog.end());

            // call on data
            plog.clear();
            strat.on_data(in, plog);

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


    void execute_orders(std::vector<OrderEvent>& orders,
                        double exec_price,
                        long long exec_ts,
                        double& cash,
                        double& opos,
                        double& eprice,
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
                    if (!sc.long_active)              { reject(ord, "longs disabled");                break; }
                    if (opos < 0)                     { reject(ord, "LONG while short");              break; }
                    if (!sc.stacking && opos > 0)     { reject(ord, "stacking disabled");             break; }
                    eprice = exec_price;
                    fill_buy(ord, exec_price, cash, opos);
                    break;

                case SIGNAL::SHORT:
                    if (!sc.short_active)             { reject(ord, "shorts disabled");               break; }
                    if (opos > 0)                     { reject(ord, "SHORT while long");              break; }
                    if (!sc.stacking && opos < 0)     { reject(ord, "stacking disabled");             break; }
                    eprice = exec_price;
                    fill_sell(ord, exec_price, cash, opos);
                    break;

                case SIGNAL::SELL:
                    if (opos <= 0)                    { reject(ord, "SELL while not long");           break; }
                    fill_sell(ord, exec_price, cash, opos);
                    if (opos == 0) eprice = 0.0;
                    break;

                case SIGNAL::COVER:
                    if (opos >= 0)                    { reject(ord, "COVER while not short");         break; }
                    fill_buy(ord, exec_price, cash, opos);
                    if (opos == 0) eprice = 0.0;
                    break;

                case SIGNAL::BBUY:
                    if (!sc.long_active && (opos >= 0 || ord.qty > -opos)) {
                        reject(ord, "longs disabled"); break;
                    }
                    if (!sc.stacking && opos > 0)     { reject(ord, "stacking disabled");             break; }
                    fill_buy(ord, exec_price, cash, opos);
                    if (opos == ord.qty) eprice = exec_price;
                    break;

                case SIGNAL::BSELL:
                    if (!sc.short_active && (opos <= 0 || ord.qty > opos)) {
                        reject(ord, "shorts disabled"); break;
                    }
                    if (!sc.stacking && opos < 0)     { reject(ord, "stacking disabled");             break; }
                    fill_sell(ord, exec_price, cash, opos);
                    if (opos == -ord.qty) eprice = exec_price;
                    break;

                default: break;
            }
        }
    }
};
