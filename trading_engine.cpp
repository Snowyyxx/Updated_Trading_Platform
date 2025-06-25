#include "trading_engine.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>

void TradingEngine::placeOrder(const Order &o) {
    {
        std::lock_guard<std::mutex> lk(books_mtx);
        // operator[] default‐constructs the OrderBook if missing
        books[o.stock_id].addOrder(o);
    }
    insert_order(o);  // DB insert (guarded by db_mutex)
}

void TradingEngine::runMatcher() {
    using namespace std::chrono_literals;
    while (true) {
        {
            std::lock_guard<std::mutex> lk(books_mtx);
            for (auto &kv : books) {
                auto &book = kv.second;
                Order buy, sell;
                while (book.matchOrder(buy, sell)) {
                    int traded = std::min(buy.units, sell.units);
                    buy.units  -= traded;
                    sell.units -= traded;
                    settleTrade(buy, sell);
                    if (buy.units  > 0) book.addOrder(buy);
                    if (sell.units > 0) book.addOrder(sell);
                }
            }
        }
        std::this_thread::sleep_for(50ms);
    }
}

void TradingEngine::settleTrade(Order &buy, Order &sell) {
    buy.status  = (buy.units  > 0 ? "Partially Executed" : "Fully Executed");
    sell.status = (sell.units > 0 ? "Partially Executed" : "Fully Executed");
    update_status(buy);
    update_status(sell);
    transaction_insert(buy, sell);
}

void TradingEngine::runMonitor() {
    using namespace std::chrono_literals;
    while (true) {
        std::this_thread::sleep_for(2s);

        // **skip** printing while user is typing
        if (input_active.load()) 
            continue;

        std::lock_guard<std::mutex> lk(books_mtx);
        std::cout << "\n=== Live Order Books ===\n";
        for (auto &kv : books) {
            int sid  = kv.first;
            auto &bk = kv.second;
            std::cout << "Stock " << sid
                      << " | Best Bid: " << bk.bestBid()
                      << " | Best Ask: " << bk.bestAsk()
                      << '\n';
        }
    }
}
