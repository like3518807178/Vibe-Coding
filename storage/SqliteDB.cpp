#include "SqliteDB.h"

#include "../common/Logger.h"

#include <string>

SqliteDB::SqliteDB() : db_(nullptr) {}

SqliteDB::~SqliteDB() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SqliteDB::open(const std::string& db_path, int busy_timeout_ms, std::string& error) {
    error.clear();

    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        error = sqlite3_errmsg(db_);
        Logger::error("sqlite3_open failed: db_path=" + db_path + ", error=" + error);
        if (db_ != nullptr) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    if (sqlite3_busy_timeout(db_, busy_timeout_ms) != SQLITE_OK) {
        error = sqlite3_errmsg(db_);
        Logger::error("sqlite3_busy_timeout failed: db_path=" + db_path + ", error=" + error);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    return true;
}

bool SqliteDB::execute(const std::string& sql, std::string& error) {
    error.clear();

    char* err_msg = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg != nullptr) {
            error = err_msg;
            sqlite3_free(err_msg);
        } else {
            error = sqlite3_errmsg(db_);
        }
        Logger::error("sqlite3_exec failed: error=" + error + ", sql=" + sql);
        return false;
    }

    return true;
}

sqlite3* SqliteDB::raw() {
    return db_;
}
