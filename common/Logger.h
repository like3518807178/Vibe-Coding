#ifndef TINYIM_COMMON_LOGGER_H
#define TINYIM_COMMON_LOGGER_H

#include <string>

enum class LogLevel {
    Info,
    Error
};

class Logger {
public:
    static void setLevel(LogLevel level);
    static LogLevel level();
    static bool parseLevel(const std::string& text, LogLevel& level);
    static void info(const std::string& message);
    static void error(const std::string& message);

private:
    static void log(LogLevel level, const std::string& message);
};

#endif
