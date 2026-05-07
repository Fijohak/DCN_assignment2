#include "logger.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

Logger::Logger(const std::string& filename) : filename(filename) {}

void Logger::info(const std::string& message) {
    write("INFO", message);
}

void Logger::error(const std::string& message) {
    write("ERROR", message);
}

void Logger::write(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::ofstream out(filename.c_str(), std::ios::out | std::ios::app);
    if (!out.is_open()) {
        return;
    }

    out << "[" << timestamp() << "] "
        << "[" << level << "] "
        << message << '\n';
}

std::string Logger::timestamp() {
    std::time_t now = std::time(NULL);
    std::tm localTime;

#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
