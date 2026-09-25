#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <mutex>

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

// RAII stream wrapper that flushes to Logger on destruction
class StreamLog {
public:
    StreamLog(LogLevel level) : level_(level) {}
    ~StreamLog();

    template<typename T>
    StreamLog& operator<<(const T& value) {
        stream_ << value;
        return *this;
    }

private:
    LogLevel level_;
    std::ostringstream stream_;
};

class Logger {
public:
    static void setLevel(LogLevel level);
    static LogLevel getLevel();

    // Stream-based logging: Logger::info() << "message" << value;
    // Internally uses ostringstream, flushes on semicolon.
    static StreamLog debug();
    static StreamLog info();
    static StreamLog warn();
    static StreamLog error();

    // Simple one-line logging
    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warn(const std::string& msg);
    static void error(const std::string& msg);

private:
    friend class StreamLog;
    static LogLevel currentLevel;
    static std::mutex logMutex;

    static void log(LogLevel level, const std::string& msg);
    static std::string getTimestamp();
    static std::string levelToString(LogLevel level);
};
