#include "adapters/sqlite/sqlite_adapter.h"
#include "errors/errors.h"
#include <iostream>

#ifdef QLE_HAVE_SQLITE3
#include <sqlite3.h>
#endif

namespace qle {
namespace adapters {

SQLiteAdapter::SQLiteAdapter() : db_(nullptr), stmt_(nullptr), has_next_(false), is_mock_(false), mock_index_(0) {
}

SQLiteAdapter::~SQLiteAdapter() {
    Close();
}

void SQLiteAdapter::Open(const std::string& source) {
#ifdef QLE_HAVE_SQLITE3
    // Open actual SQLite db
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

    // if fallback is used we could query sqlite_master to find the first table
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
#else
    // Mock implementation
    is_mock_ = true;
    std::cerr << "[Warning] SQLite3 not found during build. Using mock SQLite adapter for " << source << std::endl;
    mock_data_.push_back({{"id", "1"}, {"name", "Alice"}});
    mock_data_.push_back({{"id", "2"}, {"name", "Bob"}});
    mock_index_ = 0;
    has_next_ = mock_index_ < mock_data_.size();
#endif
}

void SQLiteAdapter::FetchNext() {
#ifdef QLE_HAVE_SQLITE3
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
#endif
}

bool SQLiteAdapter::HasNext() {
    return has_next_;
}

Row SQLiteAdapter::Next() {
    if (!has_next_) {
        throw errors::QleException("No more rows in SQLite adapter");
    }
#ifdef QLE_HAVE_SQLITE3
    if (is_mock_) {
        Row r = mock_data_[mock_index_++];
        has_next_ = mock_index_ < mock_data_.size();
        return r;
    }
    Row r = current_row_;
    FetchNext();
    return r;
#else
    Row r = mock_data_[mock_index_++];
    has_next_ = mock_index_ < mock_data_.size();
    return r;
#endif
}

void SQLiteAdapter::Close() {
#ifdef QLE_HAVE_SQLITE3
    if (stmt_) {
        sqlite3_finalize(static_cast<sqlite3_stmt*>(stmt_));
        stmt_ = nullptr;
    }
    if (db_) {
        sqlite3_close(static_cast<sqlite3*>(db_));
        db_ = nullptr;
    }
#endif
    has_next_ = false;
}

} // namespace adapters
} // namespace qle
