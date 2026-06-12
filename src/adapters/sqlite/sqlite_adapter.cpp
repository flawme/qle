#include "adapters/sqlite/sqlite_adapter.h"
#include "errors/errors.h"
#include <iostream>
#include <sqlite3.h>

namespace qle {
namespace adapters {

SQLiteAdapter::SQLiteAdapter() : db_(nullptr), stmt_(nullptr), has_next_(false) {
}

SQLiteAdapter::~SQLiteAdapter() {
    Close();
}

void SQLiteAdapter::Open(const std::string& source) {
    std::string filename = source;
    std::string table = "sqlite_master"; // fallback
    auto pos = source.find(':');
    if (pos != std::string::npos) {
        filename = source.substr(0, pos);
        table = source.substr(pos + 1);
    }
    
    sqlite3* db = nullptr;
    if (sqlite3_open(filename.c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        throw errors::QleException("Failed to open SQLite database: " + filename);
    }
    db_ = db;

    if (table == "sqlite_master" && pos == std::string::npos) {
        sqlite3_stmt* meta_stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' LIMIT 1;", -1, &meta_stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(meta_stmt) == SQLITE_ROW) {
                const char* tblName = reinterpret_cast<const char*>(sqlite3_column_text(meta_stmt, 0));
                if (tblName) table = tblName;
            }
            sqlite3_finalize(meta_stmt);
        }
    }

    std::string query = "SELECT * FROM " + table;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw errors::QleException("Failed to prepare SQLite query: " + query);
    }
    stmt_ = stmt;
    FetchNext();
}

void SQLiteAdapter::FetchNext() {
    if (!stmt_) return;
    auto stmt = static_cast<sqlite3_stmt*>(stmt_);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        has_next_ = true;
        current_row_.clear();
        int count = sqlite3_column_count(stmt);
        for (int i = 0; i < count; ++i) {
            std::string colName = sqlite3_column_name(stmt, i);
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            current_row_[colName] = val ? std::string(val) : "";
        }
    } else {
        has_next_ = false;
    }
}

bool SQLiteAdapter::HasNext() {
    return has_next_;
}

Row SQLiteAdapter::Next() {
    if (!has_next_) {
        throw errors::QleException("No more rows in SQLite adapter");
    }
    Row r = current_row_;
    FetchNext();
    return r;
}

void SQLiteAdapter::Close() {
    if (stmt_) {
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_));
        stmt_ = nullptr;
    }
    if (db_) {
        sqlite3_close(static_cast<sqlite3*>(db_));
        db_ = nullptr;
    }
    has_next_ = false;
}

} // namespace adapters
} // namespace qle
