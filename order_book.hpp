#pragma once
#include <queue>
#include <mutex>
#include "order.hpp"

// ── Comparators for priority_queues ───────────────────────
struct BuyCmp  { bool operator()(Order const &a, Order const &b) const { return a.price < b.price; } };
struct SellCmp { bool operator()(Order const &a, Order const &b) const { return a.price > b.price; } };

// ── One order book per stock ───────────────────────────────
class OrderBook {
public:
    explicit OrderBook(int sid = 0) : stock_id(sid) {}

    // Add a new order to the book
    void addOrder(const Order &o);

    // Try to pop one matching buy/sell pair. Returns true if matched.
    bool matchOrder(Order &buy, Order &sell);

    // Helpers for monitoring
    int bestBid() const;    // highest buy price or -1
    int bestAsk() const;    // lowest sell price or -1

private:
    int stock_id{0};
    std::priority_queue<Order, std::vector<Order>, BuyCmp>  buy_q;
    std::priority_queue<Order, std::vector<Order>, SellCmp> sell_q;
    mutable std::mutex                                     mtx;
};
