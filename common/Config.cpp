#include "Config.h"

#include <cctype>
#include <sstream>
#include <fstream>
#include <map>

namespace {

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return text.substr(begin, end - begin);
}

bool parseUnsigned(const std::string& text, unsigned long long& value) {
    try {
        std::size_t pos = 0;
        value = std::stoull(text, &pos);
        return pos == text.size();
    } catch (...) {
        return false;
    }
}

std::string logLevelText(LogLevel level) {
    return level == LogLevel::Info ? "INFO" : "ERROR";
}

}  // namespace

bool Config::loadFromFile(const std::string& path, AppConfig& config, std::string& error) {
    error.clear();

    std::ifstream input(path);
    if (!input.is_open()) {
        error = "cannot open config file: " + path;
        return false;
    }

    std::map<std::string, std::string> items;
    std::string line;
    int line_no = 0;
    while (std::getline(input, line)) {
        ++line_no;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const std::size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            error = "invalid config line " + std::to_string(line_no);
            return false;
        }

        const std::string key = trim(trimmed.substr(0, eq_pos));
        const std::string value = trim(trimmed.substr(eq_pos + 1));
        if (key.empty() || value.empty()) {
            error = "invalid config line " + std::to_string(line_no);
            return false;
        }

        items[key] = value;
    }

    if (items.find("port") != items.end()) {
        unsigned long long value = 0;
        if (!parseUnsigned(items["port"], value) || value > 65535ULL) {
            error = "invalid port";
            return false;
        }
        config.port = static_cast<std::uint16_t>(value);
    }

    if (items.find("db_path") != items.end()) {
        config.db_path = items["db_path"];
    }

    if (items.find("max_packet_size") != items.end()) {
        unsigned long long value = 0;
        if (!parseUnsigned(items["max_packet_size"], value) || value == 0ULL) {
            error = "invalid max_packet_size";
            return false;
        }
        config.max_packet_size = static_cast<std::uint32_t>(value);
    }

    if (items.find("idle_timeout") != items.end()) {
        unsigned long long value = 0;
        if (!parseUnsigned(items["idle_timeout"], value) || value == 0ULL) {
            error = "invalid idle_timeout";
            return false;
        }
        config.idle_timeout = static_cast<int>(value);
    }

    if (items.find("log_level") != items.end()) {
        LogLevel level = LogLevel::Info;
        if (!Logger::parseLevel(items["log_level"], level)) {
            error = "invalid log_level";
            return false;
        }
        config.log_level = level;
    }

    return true;
}

std::string Config::toDisplayString(const AppConfig& config) {
    std::ostringstream oss;
    oss << "effective config: "
        << "port=" << config.port
        << ", db_path=" << config.db_path
        << ", max_packet_size=" << config.max_packet_size
        << ", idle_timeout=" << config.idle_timeout
        << ", log_level=" << logLevelText(config.log_level);
    return oss.str();
}
