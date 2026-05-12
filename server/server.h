#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

#include "../database/course_db.h"
#include "logger.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
typedef int SOCKET;
#endif

#include <atomic>
#include <string>
#include <vector>
#include <mutex>

struct ConnectionInfo {
    std::string address;
    std::string username;
    std::string role;
    std::string connectedSince;
};

class Server {
public:
    Server(unsigned short port, unsigned short httpPort, CourseDB& database, Logger& logger);
    bool start();

    // Concurrency tracking
    void onClientConnected(const std::string& address);
    void onClientDisconnected(const std::string& address);
    void onClientLoggedIn(const std::string& address, const std::string& username, const std::string& role);
    int getActiveConnections() const;
    int getTotalConnections() const;
    std::vector<ConnectionInfo> getConnectionList() const;

private:
    unsigned short port;
    unsigned short httpPort;
    CourseDB& database;
    Logger& logger;
    SOCKET listenSocket;
    SOCKET httpListenSocket;

    // Concurrency tracking
    std::atomic<int> activeConnections;
    std::atomic<int> totalConnections;
    std::vector<ConnectionInfo> connectionList;
    mutable std::mutex connectionMutex;
    mutable std::mutex printMutex;

    bool createListenSocket();
    bool createHttpListenSocket();
    void httpServerThread();
    void handleHttpClient(SOCKET clientSocket);
    std::string handleHttpRequest(const std::string& request);
    std::string urlDecode(const std::string& input);
    std::string getHttpResponse(const std::string& body, const std::string& contentType = "text/html");
    std::string serveIndexHtml();
    std::string serveApi(const std::string& queryString);
    std::string serveStyleCss();
    std::string serveScriptJs();
    static std::string clientAddressToString(sockaddr_in clientAddr);
};

#endif  // SERVER_SERVER_H
