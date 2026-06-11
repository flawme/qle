#pragma once

#include <string>

namespace qle {

enum class LogLevel {
    INFO,
    WARNING,
    ERROR,
    DEBUG_LOG
};

class Debug {
public:
    static void Log(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);
    static void DebugLog(const std::string& message);

    static void SetLogLevel(LogLevel level);
    static void Enable(bool enable);

private:
    static LogLevel current_level_;
    static bool enabled_;

    static void WriteLog(LogLevel level, const std::string& message);
};

} // namespace qle
