#include "MessageDao.h"

#include <sqlite3.h>

namespace {

constexpr const char* kCreateOfflineMessagesSql =
    "CREATE TABLE IF NOT EXISTS offline_messages ("
    "msg_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "from_user TEXT NOT NULL,"
    "to_user TEXT NOT NULL,"
    "body TEXT NOT NULL,"
    "ts TEXT NOT NULL,"
    "delivered INTEGER NOT NULL DEFAULT 0"
    ");";

}  // namespace

MessageDao::MessageDao(SqliteDB& db) : db_(db) {}

bool MessageDao::createTable(std::string& error) {
    return db_.execute(kCreateOfflineMessagesSql, error);
}

bool MessageDao::insertOffline(
    const std::string& from_user,
    const std::string& to_user,
    const std::string& body,
    const std::string& ts,
    std::int64_t& msg_id,
    std::string& error
) {
    error.clear();
    msg_id = 0;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO offline_messages(from_user, to_user, body, ts, delivered) "
        "VALUES(?, ?, ?, ?, 0);";
    const int prepare_rc = sqlite3_prepare_v2(db_.raw(), sql, -1, &stmt, nullptr);
    if (prepare_rc != SQLITE_OK) {
        error = sqlite3_errmsg(db_.raw());
        return false;
    }

    sqlite3_bind_text(stmt, 1, from_user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, to_user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, body.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, ts.c_str(), -1, SQLITE_TRANSIENT);

    const int step_rc = sqlite3_step(stmt);
    if (step_rc != SQLITE_DONE) {
        error = sqlite3_errmsg(db_.raw());
        sqlite3_finalize(stmt);
        return false;
    }

    msg_id = sqlite3_last_insert_rowid(db_.raw());
    sqlite3_finalize(stmt);
    return true;
}

bool MessageDao::listUndelivered(
    const std::string& to_user,
    std::vector<OfflineMessageRecord>& records,
    std::string& error
) {
    records.clear();
    error.clear();

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT msg_id, from_user, to_user, body, ts, delivered "
        "FROM offline_messages "
        "WHERE to_user = ? AND delivered = 0 "
        "ORDER BY msg_id ASC;";
    const int prepare_rc = sqlite3_prepare_v2(db_.raw(), sql, -1, &stmt, nullptr);
    if (prepare_rc != SQLITE_OK) {
        error = sqlite3_errmsg(db_.raw());
        return false;
    }

    sqlite3_bind_text(stmt, 1, to_user.c_str(), -1, SQLITE_TRANSIENT);

    while (true) {
        const int step_rc = sqlite3_step(stmt);
        if (step_rc == SQLITE_ROW) {
            OfflineMessageRecord record{};
            record.msg_id = sqlite3_column_int64(stmt, 0);
            record.from_user = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            record.to_user = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            record.body = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            record.ts = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            record.delivered = sqlite3_column_int(stmt, 5) != 0;
            records.push_back(record);
            continue;
        }

        if (step_rc == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return true;
        }

        error = sqlite3_errmsg(db_.raw());
        sqlite3_finalize(stmt);
        return false;
    }
}

bool MessageDao::markDelivered(std::int64_t msg_id, std::string& error) {
    error.clear();

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE offline_messages SET delivered = 1 WHERE msg_id = ?;";
    const int prepare_rc = sqlite3_prepare_v2(db_.raw(), sql, -1, &stmt, nullptr);
    if (prepare_rc != SQLITE_OK) {
        error = sqlite3_errmsg(db_.raw());
        return false;
    }

    sqlite3_bind_int64(stmt, 1, msg_id);

    const int step_rc = sqlite3_step(stmt);
    if (step_rc != SQLITE_DONE) {
        error = sqlite3_errmsg(db_.raw());
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}
