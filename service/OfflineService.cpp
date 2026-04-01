#include "OfflineService.h"

#include "SessionManager.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {

constexpr int kBusyTimeoutMs = 3000;

}  // namespace

OfflineService::OfflineService(std::string db_path, SessionManager& session_manager)
    : db_path_(std::move(db_path)),
      session_manager_(session_manager),
      message_dao_(db_) {}

bool OfflineService::init(std::string& error) {
    if (!db_.open(db_path_, kBusyTimeoutMs, error)) {
        return false;
    }

    return message_dao_.createTable(error);
}

std::string OfflineService::currentTimestamp() const {
    const std::time_t now = std::time(nullptr);
    std::tm local_tm{};
    localtime_r(&now, &local_tm);

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool OfflineService::handleSend(
    const std::string& from_user,
    const std::string& to_user,
    const std::string& text,
    SendDispatchResult& result,
    std::string& error
) {
    result = SendDispatchResult{false, false, false, -1, "", {}};
    error.clear();

    const std::string ts = currentTimestamp();
    if (session_manager_.isUserOnline(to_user)) {
        result.success = true;
        result.deliver_online = true;
        result.target_fd = session_manager_.getFdByUser(to_user);
        result.message = "已实时投递";
        result.deliver_payload = {
            {"from", from_user},
            {"text", text},
            {"to", to_user},
            {"ts", ts},
            {"type", "recv"}
        };
        return true;
    }

    std::int64_t msg_id = 0;
    if (!message_dao_.insertOffline(from_user, to_user, text, ts, msg_id, error)) {
        return false;
    }

    result.success = true;
    result.stored_offline = true;
    result.message = "已入离线";
    result.deliver_payload = {
        {"from", from_user},
        {"msg_id", std::to_string(msg_id)},
        {"text", text},
        {"to", to_user},
        {"ts", ts},
        {"type", "recv"}
    };
    return true;
}

bool OfflineService::listUndelivered(
    const std::string& username,
    std::vector<OfflineMessageRecord>& records,
    std::string& error
) {
    return message_dao_.listUndelivered(username, records, error);
}

bool OfflineService::markDelivered(std::int64_t msg_id, std::string& error) {
    return message_dao_.markDelivered(msg_id, error);
}
