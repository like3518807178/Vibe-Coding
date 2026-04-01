#ifndef TINYIM_STORAGE_MESSAGE_DAO_H
#define TINYIM_STORAGE_MESSAGE_DAO_H

#include "SqliteDB.h"

#include <cstdint>
#include <string>
#include <vector>

struct OfflineMessageRecord {
    std::int64_t msg_id;
    std::string from_user;
    std::string to_user;
    std::string body;
    std::string ts;
    bool delivered;
};

class MessageDao {
public:
    explicit MessageDao(SqliteDB& db);

    bool createTable(std::string& error);
    bool insertOffline(
        const std::string& from_user,
        const std::string& to_user,
        const std::string& body,
        const std::string& ts,
        std::int64_t& msg_id,
        std::string& error
    );
    bool listUndelivered(
        const std::string& to_user,
        std::vector<OfflineMessageRecord>& records,
        std::string& error
    );
    bool markDelivered(std::int64_t msg_id, std::string& error);

private:
    SqliteDB& db_;
};

#endif
