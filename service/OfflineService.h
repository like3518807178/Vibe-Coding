#ifndef TINYIM_SERVICE_OFFLINE_SERVICE_H
#define TINYIM_SERVICE_OFFLINE_SERVICE_H

#include "../storage/MessageDao.h"
#include "../storage/SqliteDB.h"

#include <map>
#include <string>
#include <vector>

class SessionManager;

struct SendDispatchResult {
    bool success;
    bool deliver_online;
    bool stored_offline;
    int target_fd;
    std::string message;
    std::map<std::string, std::string> deliver_payload;
};

class OfflineService {
public:
    OfflineService(std::string db_path, SessionManager& session_manager);

    bool init(std::string& error);
    bool handleSend(
        const std::string& from_user,
        const std::string& to_user,
        const std::string& text,
        SendDispatchResult& result,
        std::string& error
    );
    bool listUndelivered(
        const std::string& username,
        std::vector<OfflineMessageRecord>& records,
        std::string& error
    );
    bool markDelivered(std::int64_t msg_id, std::string& error);

private:
    std::string currentTimestamp() const;

    std::string db_path_;
    SessionManager& session_manager_;
    SqliteDB db_;
    MessageDao message_dao_;
};

#endif
