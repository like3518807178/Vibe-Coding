#ifndef TINYIM_SERVICE_AUTH_SERVICE_H
#define TINYIM_SERVICE_AUTH_SERVICE_H

#include "../storage/SqliteDB.h"
#include "../storage/UserDao.h"

#include <map>
#include <string>

struct AuthResult {
    bool handled;
    bool success;
    std::string type;
    std::string message;
    std::string username;
};

class AuthService {
public:
    explicit AuthService(std::string db_path);

    bool init(std::string& error);
    AuthResult handleMessage(const std::map<std::string, std::string>& fields);

    std::string db_path_;
    SqliteDB db_;
    UserDao user_dao_;
};

#endif
