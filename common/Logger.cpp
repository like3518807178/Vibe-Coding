#include "Logger.h"

#include <ctime>
#include <iostream>
#include <mutex>
#include <sstream>

namespace {

std::mutex& logMutex() {
    static std::mutex mutex;
    return mutex;
}

LogLevel& currentLevel() {
    static LogLevel level = LogLevel::Info;
    return level;
}

std::string levelText(LogLevel level) {
    return level == LogLevel::Info ? "INFO" : "ERROR";
}

std::string currentTimestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm local_tm{};
    localtime_r(&now, &local_tm);

    std::ostringstream oss;
    oss << (local_tm.tm_year + 1900) << "-";
    if (local_tm.tm_mon + 1 < 10) {
        oss << "0";
    }
    oss << (local_tm.tm_mon + 1) << "-";
    if (local_tm.tm_mday < 10) {
        oss << "0";
    }
    oss << local_tm.tm_mday << " ";
    if (local_tm.tm_hour < 10) {
        oss << "0";
    }
    oss << local_tm.tm_hour << ":";
    if (local_tm.tm_min < 10) {
        oss << "0";
    }
    oss << local_tm.tm_min << ":";
    if (local_tm.tm_sec < 10) {
        oss << "0";
    }
    oss << local_tm.tm_sec;
    return oss.str();
}

}  // namespace

void Logger::setLevel(LogLevel level) {
    currentLevel() = level;
}

LogLevel Logger::level() {
    return currentLevel();
}

bool Logger::parseLevel(const std::string& text, LogLevel& level) {
    if (text == "INFO") {
        level = LogLevel::Info;
        return true;
    }

    if (text == "ERROR") {
        level = LogLevel::Error;
        return true;
    }

    return false;
}

void Logger::info(const std::string& message) {
    log(LogLevel::Info, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::Error, message);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level == LogLevel::Info && currentLevel() == LogLevel::Error) {
        return;
    }

    std::lock_guard<std::mutex> lock(logMutex());
    std::ostream& out = level == LogLevel::Error ? std::cerr : std::cout;
    out << "[" << currentTimestamp() << "]"
        << "[" << levelText(level) << "] "
        << message << std::endl;
}
