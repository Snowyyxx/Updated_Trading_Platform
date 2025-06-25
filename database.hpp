#pragma once
#include <mutex>
#include <nlohmann/json.hpp>
#include "order.hpp"

using json = nlohmann::json;

// Shared DB mutex
extern std::mutex db_mutex;

// Return value changed to int: the newly assigned order_id
int insert_order(const Order &o);

void update_status(const Order &o);
void transaction_insert(const Order &buy, const Order &sell);
json displayOrders();
