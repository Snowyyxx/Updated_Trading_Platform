#pragma once
#include <string>

// ── Simple POD for an order ────────────────────────────────
struct Order {
    int         order_id;
    int         stock_id;
    int         user_id;
    int         units;
    int         price;        // price in integer units (e.g., cents)
    bool        is_buy;       // true = BUY, false = SELL
    std::string status;       // "Placed", "Partially Executed", "Fully Executed"
};
