#ifndef TINYIM_STORAGE_USER_DAO_H
#define TINYIM_STORAGE_USER_DAO_H

#include "SqliteDB.h"

#include <string>

class UserDao {
public:
    explicit UserDao(SqliteDB& db);

    bool createTable(std::string& error);
    bool createUser(const std::string& username, const std::string& password, std::string& error);
    bool verifyUser(const std::string& username, const std::string& password, bool& matched, std::string& error);

private:
    SqliteDB& db_;
};

#endif
