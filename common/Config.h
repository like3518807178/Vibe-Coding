#ifndef TINYIM_COMMON_CONFIG_H
#define TINYIM_COMMON_CONFIG_H

#include "Logger.h"

#include <cstdint>
#include <string>

struct AppConfig {
    std::uint16_t port = 7777;
    std::string db_path = "tinyim_v8.db";
    std::uint32_t max_packet_size = 1024 * 1024;
    int idle_timeout = 5;
    LogLevel log_level = LogLevel::Info;
};

class Config {
public:
    static bool loadFromFile(const std::string& path, AppConfig& config, std::string& error);
    static std::string toDisplayString(const AppConfig& config);
};

#endif
