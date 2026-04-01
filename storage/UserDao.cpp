#include "UserDao.h"

#include <sqlite3.h>

namespace {

constexpr const char* kCreateUsersTableSql =
    "CREATE TABLE IF NOT EXISTS users ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "username TEXT NOT NULL UNIQUE,"
    "password TEXT NOT NULL"
    ");";

}  // namespace

UserDao::UserDao(SqliteDB& db) : db_(db) {}

bool UserDao::createTable(std::string& error) {
    return db_.execute(kCreateUsersTableSql, error);
}

bool UserDao::createUser(const std::string& username, const std::string& password, std::string& error) {
    error.clear();

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO users(username, password) VALUES(?, ?);";
    const int prepare_rc = sqlite3_prepare_v2(db_.raw(), sql, -1, &stmt, nullptr);
    if (prepare_rc != SQLITE_OK) {
        error = sqlite3_errmsg(db_.raw());
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);

    const int step_rc = sqlite3_step(stmt);
    if (step_rc != SQLITE_DONE) {
        if (step_rc == SQLITE_CONSTRAINT) {
            error = "用户已存在";
        } else {
            error = sqlite3_errmsg(db_.raw());
        }
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool UserDao::verifyUser(
    const std::string& username,
    const std::string& password,
    bool& matched,
    std::string& error
) {
    matched = false;
    error.clear();

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT password FROM users WHERE username = ?;";
    const int prepare_rc = sqlite3_prepare_v2(db_.raw(), sql, -1, &stmt, nullptr);
    if (prepare_rc != SQLITE_OK) {
        error = sqlite3_errmsg(db_.raw());
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    const int step_rc = sqlite3_step(stmt);
    if (step_rc == SQLITE_ROW) {
        const unsigned char* db_password = sqlite3_column_text(stmt, 0);
        if (db_password != nullptr && password == reinterpret_cast<const char*>(db_password)) {
            matched = true;
        } else {
            error = "用户名或密码错误";
        }
        sqlite3_finalize(stmt);
        return true;
    }

    if (step_rc == SQLITE_DONE) {
        error = "用户名或密码错误";
        sqlite3_finalize(stmt);
        return true;
    }

    error = sqlite3_errmsg(db_.raw());
    sqlite3_finalize(stmt);
    return false;
}
