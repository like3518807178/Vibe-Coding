#ifndef TINYIM_SERVICE_AUTH_SERVICE_H
#define TINYIM_SERVICE_AUTH_SERVICE_H

#include "../storage/SqliteDB.h"
#include "../storage/UserDao.h"

#include <map>
#include <string>

class AuthService {
public:
    explicit AuthService(std::string db_path);

    bool init(std::string& error);
    std::map<std::string, std::string> handleMessage(
        const std::map<std::string, std::string>& fields,
        bool& handled
    );

    std::string db_path_;
    SqliteDB db_;
    UserDao user_dao_;
};

#endif
