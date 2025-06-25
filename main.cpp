#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <sqlite3.h>
#include <unordered_map>
#include <string>

#include "order.hpp"
#include "order_book.hpp"
#include "database.hpp"
#include "trading_engine.hpp"

// Global DB mutex
extern std::mutex db_mutex;

// Not used in these modes, but needed by trading_engine.cpp
std::atomic<bool> input_active{false};

namespace ModeHelpers {
    inline void sleep_ms(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    void runMonitorMode() {
        while (true) {
            auto arr = displayOrders();
            struct Stat { int bestBid = -1, bestAsk = -1; };
            std::unordered_map<int, Stat> stats;

            for (auto &o : arr) {
                int sid   = o["stock_id"];
                int price = o["price"];
                auto &S = stats[sid];
                if (o["status"] == "Placed" || o["status"] == "Partially Executed") {
                    if (o["order_type"] == "BUY") {
                        S.bestBid = std::max(S.bestBid, price);
                    } else {
                        if (S.bestAsk < 0 || price < S.bestAsk)
                            S.bestAsk = price;
                    }
                }
            }

            std::cout << "\n=== MONITOR (best bid/ask) ===\n";
            for (auto &kv : stats) {
                std::cout << "Stock " << kv.first
                          << " | Bid: " << (kv.second.bestBid  < 0 ? 0 : kv.second.bestBid)
                          << " | Ask: " << (kv.second.bestAsk  < 0 ? 0 : kv.second.bestAsk)
                          << "\n";
            }
            sleep_ms(2000);
        }
    }

    void runTransactionsMode() {
        int last_id = 0;
        while (true) {
            {
                std::lock_guard<std::mutex> lk(db_mutex);
                sqlite3 *db = nullptr;
                if (sqlite3_open("stock_exchange.db", &db) == SQLITE_OK) {
                    const char *sql =
                        "SELECT id, buy_order_id, sell_order_id, stock_id, units, sell_order_price "
                        "FROM transactions WHERE id > ? ORDER BY id;";
                    sqlite3_stmt *stmt = nullptr;
                    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
                    sqlite3_bind_int(stmt, 1, last_id);

                    while (sqlite3_step(stmt) == SQLITE_ROW) {
                        last_id = sqlite3_column_int(stmt, 0);
                        std::cout << "[TX " << last_id << "] "
                                  << "Stock " << sqlite3_column_int(stmt, 3)
                                  << " | Qty " << sqlite3_column_int(stmt, 4)
                                  << " @ "   << sqlite3_column_int(stmt, 5)
                                  << " (B#" << sqlite3_column_int(stmt, 1)
                                  << " / S#" << sqlite3_column_int(stmt, 2)
                                  << ")\n";
                    }
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                }
            }
            sleep_ms(1000);
        }
    }

    void runDbViewMode() {
        while (true) {
            {
                auto orders = displayOrders();
                std::cout << "\n--- order_records ---\n";
                for (auto &o : orders) {
                    std::cout << "ID#"   << o["order_id"]
                              << " | S"   << o["stock_id"]
                              << " | U"   << o["user_id"]
                              << " | "    << o["units"]
                              << " @ "    << o["price"]
                              << " | "    << o["status"]
                              << " / "    << o["order_type"]
                              << "\n";
                }
            }
            {
                std::lock_guard<std::mutex> lk(db_mutex);
                sqlite3 *db = nullptr;
                sqlite3_open("stock_exchange.db", &db);

                std::cout << "\n--- transactions ---\n";
                const char *sql =
                    "SELECT id, buy_order_id, sell_order_id, stock_id, units, sell_order_price "
                    "FROM transactions ORDER BY id;";
                sqlite3_stmt *stmt = nullptr;
                sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    std::cout << "TX#" << sqlite3_column_int(stmt, 0)
                              << " | B#" << sqlite3_column_int(stmt, 1)
                              << " / S#" << sqlite3_column_int(stmt, 2)
                              << " | S"  << sqlite3_column_int(stmt, 3)
                              << " | "   << sqlite3_column_int(stmt, 4)
                              << " @ "   << sqlite3_column_int(stmt, 5)
                              << "\n";
                }
                sqlite3_finalize(stmt);
                sqlite3_close(db);
            }
            sleep_ms(3000);
        }
    }

    void runPricesMode() {
        while (true) {
            struct P { int sid, price; };
            std::vector<P> v;

            {
                std::lock_guard<std::mutex> lk(db_mutex);
                sqlite3 *db = nullptr;
                sqlite3_open("stock_exchange.db", &db);

                const char *sql =
                    "SELECT stock_id, sell_order_price "
                    "FROM ("
                    "  SELECT *, ROW_NUMBER() OVER (PARTITION BY stock_id ORDER BY id DESC) rn "
                    "  FROM transactions"
                    ") "
                    "WHERE rn = 1;";
                sqlite3_stmt *stmt = nullptr;
                sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    v.push_back({ sqlite3_column_int(stmt, 0),
                                  sqlite3_column_int(stmt, 1) });
                }
                sqlite3_finalize(stmt);
                sqlite3_close(db);
            }

            std::sort(v.begin(), v.end(),
                      [](auto &a, auto &b){ return a.price > b.price; });

            std::cout << "\n*** Stock Prices (Latest) ***\n";
            for (auto &p : v) {
                std::cout << "Stock " << p.sid
                          << " → " << p.price << "\n";
            }
            sleep_ms(3000);
        }
    }
} // namespace ModeHelpers

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <mode>\nModes: engine | buy | sell | monitor | transactions | db | prices\n";
        return 1;
    }

    std::string mode(argv[1]);
    if (mode == "engine") {
        TradingEngine engine;
        engine.runMatcher();
    }
    else if (mode == "buy" || mode == "sell") {
        bool is_buy = (mode == "buy");
        while (true) {
            int stock_id, units, price, user_id;
            std::cout << "[" << (is_buy ? "BUY" : "SELL") << "] "
                      << "Enter: stock_id units price user_id → " << std::flush;
            if (!(std::cin >> stock_id >> units >> price >> user_id))
                break;

            Order o;
            // Let the DB assign a unique order_id:
            o.stock_id = stock_id;
            o.units    = units;
            o.price    = price;
            o.user_id  = user_id;
            o.is_buy   = is_buy;
            o.status   = "Placed";

            int assigned_id = insert_order(o);
            if (assigned_id > 0) {
                std::cout << (is_buy ? "\033[32m" : "\033[31m")
                          << (is_buy ? "Buy " : "Sell ") << "order #"
                          << assigned_id << "\033[0m\n";
            }
        }
    }
    else if (mode == "monitor")       ModeHelpers::runMonitorMode();
    else if (mode == "transactions")  ModeHelpers::runTransactionsMode();
    else if (mode == "db")            ModeHelpers::runDbViewMode();
    else if (mode == "prices")        ModeHelpers::runPricesMode();
    else {
        std::cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }

    return 0;
}
