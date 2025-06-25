#pragma once
#include <unordered_map>
#include <mutex>
#include <atomic>
#include "order_book.hpp"
#include "database.hpp"

// Global flag (defined in main.cpp) that indicates when the CLI is reading.
extern std::atomic<bool> input_active;

class TradingEngine {
public:
    TradingEngine() = default;

    // called by CLI thread
    void placeOrder(const Order &o);

    // background threads
    void runMatcher();   // matches orders forever
    void runMonitor();   // prints best bid/ask every 2s, but only when input_active==false

private:
    std::unordered_map<int, OrderBook> books;
    std::mutex                       books_mtx;

    // after a match, update DB & possibly re‐queue residuals
    void settleTrade(Order &buy, Order &sell);
};
