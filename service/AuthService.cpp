#include "AuthService.h"

#include <utility>

namespace {

constexpr int kBusyTimeoutMs = 3000;

}  // namespace

AuthService::AuthService(std::string db_path)
    : db_path_(std::move(db_path)), user_dao_(db_) {}

bool AuthService::init(std::string& error) {
    if (!db_.open(db_path_, kBusyTimeoutMs, error)) {
        return false;
    }

    return user_dao_.createTable(error);
}

std::map<std::string, std::string> AuthService::handleMessage(
    const std::map<std::string, std::string>& fields,
    bool& handled
) {
    handled = false;

    auto type_it = fields.find("type");
    if (type_it == fields.end()) {
        return {};
    }

    const std::string& type = type_it->second;
    if (type != "register" && type != "login") {
        return {};
    }

    handled = true;

    auto username_it = fields.find("username");
    auto password_it = fields.find("password");
    if (username_it == fields.end() || password_it == fields.end()) {
        return {
            {"message", "缺少 username 或 password"},
            {"ok", "false"},
            {"type", type + "_resp"}
        };
    }

    if (type == "register") {
        std::string error;
        if (!user_dao_.createUser(username_it->second, password_it->second, error)) {
            return {
                {"message", error.empty() ? "注册失败" : error},
                {"ok", "false"},
                {"type", "register_resp"}
            };
        }

        return {
            {"message", "注册成功"},
            {"ok", "true"},
            {"type", "register_resp"}
        };
    }

    bool matched = false;
    std::string error;
    if (!user_dao_.verifyUser(username_it->second, password_it->second, matched, error)) {
        return {
            {"message", error.empty() ? "登录失败" : error},
            {"ok", "false"},
            {"type", "login_resp"}
        };
    }

    if (!matched) {
        return {
            {"message", error.empty() ? "用户名或密码错误" : error},
            {"ok", "false"},
            {"type", "login_resp"}
        };
    }

    return {
        {"message", "登录成功"},
        {"ok", "true"},
        {"type", "login_resp"}
    };
}
