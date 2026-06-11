#include "logger/logger.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>

namespace qle {

LogLevel Debug::current_level_ = LogLevel::DEBUG_LOG;
bool Debug::enabled_ = true;

void Debug::SetLogLevel(LogLevel level) {
    current_level_ = level;
}

void Debug::Enable(bool enable) {
    enabled_ = enable;
}

void Debug::Log(const std::string& message) {
    WriteLog(LogLevel::INFO, message);
}

void Debug::Warning(const std::string& message) {
    WriteLog(LogLevel::WARNING, message);
}

void Debug::Error(const std::string& message) {
    WriteLog(LogLevel::ERROR, message);
}

void Debug::DebugLog(const std::string& message) {
    WriteLog(LogLevel::DEBUG_LOG, message);
}

void Debug::WriteLog(LogLevel level, const std::string& message) {
    if (!enabled_ || level > current_level_) {
        return;
    }

    std::string prefix;
    switch (level) {
        case LogLevel::INFO: prefix = "[INFO]"; break;
        case LogLevel::WARNING: prefix = "[WARN]"; break;
        case LogLevel::ERROR: prefix = "[ERR ]"; break;
        case LogLevel::DEBUG_LOG: prefix = "[DBUG]"; break;
    }

    if (level == LogLevel::ERROR) {
        std::cerr << prefix << " " << message << std::endl;
    } else {
        std::cout << prefix << " " << message << std::endl;
    }
}

} // namespace qle
