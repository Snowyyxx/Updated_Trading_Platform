#include "database.hpp"
#include <sqlite3.h>
#include <iostream>
#include <sstream>

std::mutex db_mutex;

int insert_order(const Order &o) {
    std::lock_guard<std::mutex> lock(db_mutex);
    sqlite3 *db = nullptr;
    if (sqlite3_open("stock_exchange.db", &db) != SQLITE_OK) {
        std::cerr << "[DB] open error: " << sqlite3_errmsg(db) << std::endl;
        return -1;
    }

    // Omit order_id so SQLite auto-assigns a UNIQUE PK
    const char *sql =
      "INSERT INTO order_records "
      "(stock_id, user_id, units, price, status, order_type) "
      "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[DB] prepare error: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, o.stock_id);
    sqlite3_bind_int(stmt, 2, o.user_id);
    sqlite3_bind_int(stmt, 3, o.units);
    sqlite3_bind_int(stmt, 4, o.price);
    sqlite3_bind_text(stmt,5, o.status.c_str(), -1, SQLITE_TRANSIENT);
    const char *otype = o.is_buy ? "BUY" : "SELL";
    sqlite3_bind_text(stmt,6, otype, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "[DB] step error: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }

    sqlite3_finalize(stmt);
    // Grab the auto-assigned rowid (order_id)
    int new_id = static_cast<int>(sqlite3_last_insert_rowid(db));
    sqlite3_close(db);
    return new_id;
}

void update_status(const Order &o) {
    std::lock_guard<std::mutex> lock(db_mutex);
    sqlite3 *db = nullptr;
    if (sqlite3_open("stock_exchange.db", &db) != SQLITE_OK) return;

    const char *sql =
      "UPDATE order_records SET status = ? WHERE order_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, o.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  2, o.order_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void transaction_insert(const Order &buy, const Order &sell) {
    std::lock_guard<std::mutex> lock(db_mutex);
    sqlite3 *db = nullptr;
    sqlite3_open("stock_exchange.db", &db);
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    const char *sql =
      "INSERT INTO transactions "
      "(buy_order_id, sell_order_id, stock_id, units, buy_order_price, sell_order_price) "
      "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    int traded = std::min(buy.units, sell.units);
    sqlite3_bind_int(stmt, 1, buy.order_id);
    sqlite3_bind_int(stmt, 2, sell.order_id);
    sqlite3_bind_int(stmt, 3, buy.stock_id);
    sqlite3_bind_int(stmt, 4, traded);
    sqlite3_bind_int(stmt, 5, buy.price);
    sqlite3_bind_int(stmt, 6, sell.price);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
}

json displayOrders() {
    std::lock_guard<std::mutex> lock(db_mutex);
    sqlite3 *db = nullptr;
    json arr = json::array();

    if (sqlite3_open("stock_exchange.db", &db) != SQLITE_OK) {
        sqlite3_close(db);
        return arr;
    }

    const char *sql = "SELECT order_id, stock_id, user_id, units, price, status, order_type FROM order_records;";
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json o;
        o["order_id"]   = sqlite3_column_int(stmt, 0);
        o["stock_id"]   = sqlite3_column_int(stmt, 1);
        o["user_id"]    = sqlite3_column_int(stmt, 2);
        o["units"]      = sqlite3_column_int(stmt, 3);
        o["price"]      = sqlite3_column_int(stmt, 4);
        o["status"]     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        o["order_type"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        arr.push_back(o);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return arr;
}
