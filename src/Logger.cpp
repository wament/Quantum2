#include "Logger.hpp"

LogLevel Logger::currentLevel = LogLevel::DEBUG;
std::mutex Logger::logMutex;

StreamLog::~StreamLog() {
    if (Logger::getLevel() <= level_) {
        Logger::log(level_, stream_.str());
    }
}

void Logger::setLevel(LogLevel level) {
    currentLevel = level;
}

LogLevel Logger::getLevel() {
    return currentLevel;
}

StreamLog Logger::debug() { return StreamLog(LogLevel::DEBUG); }
StreamLog Logger::info() { return StreamLog(LogLevel::INFO); }
StreamLog Logger::warn() { return StreamLog(LogLevel::WARN); }
StreamLog Logger::error() { return StreamLog(LogLevel::ERROR); }

void Logger::debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
void Logger::info(const std::string& msg) { log(LogLevel::INFO, msg); }
void Logger::warn(const std::string& msg) { log(LogLevel::WARN, msg); }
void Logger::error(const std::string& msg) { log(LogLevel::ERROR, msg); }

void Logger::log(LogLevel level, const std::string& msg) {
    if (level < currentLevel) return;

    std::lock_guard<std::mutex> lock(logMutex);
    std::cout << "[" << getTimestamp() << "] [" << levelToString(level) << "] " << msg << "\n";
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.'
        << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "???";
    }
}