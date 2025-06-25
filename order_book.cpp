#include "order_book.hpp"

void OrderBook::addOrder(const Order &o) {
    std::lock_guard<std::mutex> lk(mtx);
    if (o.is_buy)   buy_q.push(o);
    else            sell_q.push(o);
}

bool OrderBook::matchOrder(Order &buy, Order &sell) {
    std::lock_guard<std::mutex> lk(mtx);
    if (buy_q.empty() || sell_q.empty()) return false;
    // Only match if highest bid >= lowest ask
    if (buy_q.top().price < sell_q.top().price) return false;

    buy  = buy_q.top();  buy_q.pop();
    sell = sell_q.top(); sell_q.pop();
    return true;
}

int OrderBook::bestBid() const {
    std::lock_guard<std::mutex> lk(mtx);
    return buy_q.empty() ? -1 : buy_q.top().price;
}

int OrderBook::bestAsk() const {
    std::lock_guard<std::mutex> lk(mtx);
    return sell_q.empty() ? -1 : sell_q.top().price;
}
