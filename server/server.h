#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

#include "../database/course_db.h"
#include "logger.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
typedef int SOCKET;
#endif

#include <atomic>

class Server {
public:
    Server(unsigned short port, CourseDB& database, Logger& logger);
    bool start();

    // Connection tracking
    void incrementConnections();
    void decrementConnections();
    int getActiveConnections() const;
    int getTotalConnections() const;

private:
    unsigned short port;
    CourseDB& database;
    Logger& logger;
    SOCKET listenSocket;

    std::atomic<int> activeConnections{0};
    std::atomic<int> totalConnections{0};

    bool createListenSocket();
    static std::string clientAddressToString(sockaddr_in clientAddr);

    // HTTP server
    void httpServerThread();
    std::string handleHttpRequest(const std::string& method, const std::string& path, const std::string& query);
    std::string urlDecode(const std::string& input);
    std::string serveStaticFile(const std::string& path);
};

#endif  // SERVER_SERVER_H
