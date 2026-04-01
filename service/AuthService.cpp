#include "AuthService.h"

#include "../common/Logger.h"

#include <utility>

namespace {

constexpr int kBusyTimeoutMs = 3000;

}  // namespace

AuthService::AuthService(std::string db_path)
    : db_path_(std::move(db_path)), user_dao_(db_) {}

bool AuthService::init(std::string& error) {
    if (!db_.open(db_path_, kBusyTimeoutMs, error)) {
        Logger::error("Open auth DB failed: db_path=" + db_path_ + ", error=" + error);
        return false;
    }

    return user_dao_.createTable(error);
}

AuthResult AuthService::handleMessage(const std::map<std::string, std::string>& fields) {
    AuthResult result{false, false, "", "", ""};

    auto type_it = fields.find("type");
    if (type_it == fields.end()) {
        return result;
    }

    const std::string& type = type_it->second;
    if (type != "register" && type != "login") {
        return result;
    }

    result.handled = true;
    result.type = type + "_resp";

    auto username_it = fields.find("username");
    auto password_it = fields.find("password");
    if (username_it == fields.end() || password_it == fields.end()) {
        result.message = "缺少 username 或 password";
        return result;
    }

    result.username = username_it->second;

    if (type == "register") {
        std::string error;
        if (!user_dao_.createUser(username_it->second, password_it->second, error)) {
            Logger::error("Register DB operation failed: user=" + username_it->second + ", error=" + error);
            result.message = error.empty() ? "注册失败" : error;
            return result;
        }

        result.success = true;
        result.message = "注册成功";
        return result;
    }

    bool matched = false;
    std::string error;
    if (!user_dao_.verifyUser(username_it->second, password_it->second, matched, error)) {
        Logger::error("Login DB operation failed: user=" + username_it->second + ", error=" + error);
        result.message = error.empty() ? "登录失败" : error;
        return result;
    }

    if (!matched) {
        result.message = error.empty() ? "用户名或密码错误" : error;
        return result;
    }

    result.success = true;
    result.message = "登录成功";
    return result;
}
