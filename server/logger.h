#ifndef SERVER_LOGGER_H
#define SERVER_LOGGER_H

#include <mutex>
#include <string>

class Logger {
public:
    explicit Logger(const std::string& filename);

    void info(const std::string& message);
    void error(const std::string& message);

private:
    std::string filename;
    std::mutex logMutex;

    void write(const std::string& level, const std::string& message);
    static std::string timestamp();
};

#endif  // SERVER_LOGGER_H
