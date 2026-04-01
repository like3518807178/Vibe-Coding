#ifndef TINYIM_STORAGE_SQLITE_DB_H
#define TINYIM_STORAGE_SQLITE_DB_H

#include <sqlite3.h>

#include <string>

class SqliteDB {
public:
    SqliteDB();
    ~SqliteDB();

    bool open(const std::string& db_path, int busy_timeout_ms, std::string& error);
    bool execute(const std::string& sql, std::string& error);
    sqlite3* raw();

private:
    sqlite3* db_;
};

#endif
