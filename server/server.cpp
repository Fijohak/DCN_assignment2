#include "server.h"

#include "client_handler.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

#include <cerrno>
#include <cstring>
#include <sstream>
#include <thread>

namespace {

std::string socketErrorText() {
#ifdef _WIN32
    return "WSA error " + std::to_string(WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

}  // namespace

Server::Server(unsigned short port, CourseDB& database, Logger& logger)
    : port(port),
      database(database),
      logger(logger),
      listenSocket(INVALID_SOCKET) {}

bool Server::start() {
    if (!createListenSocket()) {
        return false;
    }

    logger.info("Server listening on port " + std::to_string(port));

    while (true) {
        sockaddr_in clientAddr;
        std::memset(&clientAddr, 0, sizeof(clientAddr));
#ifdef _WIN32
        int clientSize = sizeof(clientAddr);
#else
        socklen_t clientSize = sizeof(clientAddr);
#endif

        SOCKET clientSocket = accept(listenSocket,
                                     reinterpret_cast<sockaddr*>(&clientAddr),
                                     &clientSize);
        if (clientSocket == INVALID_SOCKET) {
            logger.error("accept failed");
            continue;
        }

        const std::string address = clientAddressToString(clientAddr);
        std::thread([this, clientSocket, address]() {
            ClientHandler(clientSocket, database, logger, address)();
        }).detach();
    }

    return true;
}

bool Server::createListenSocket() {
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        logger.error("socket creation failed: " + socketErrorText());
        return false;
    }

    int reuseAddress = 1;
    setsockopt(listenSocket,
               SOL_SOCKET,
               SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuseAddress),
               sizeof(reuseAddress));

    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        logger.error("bind failed: " + socketErrorText());
#ifdef _WIN32
        closesocket(listenSocket);
#else
        close(listenSocket);
#endif
        listenSocket = INVALID_SOCKET;
        return false;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        logger.error("listen failed: " + socketErrorText());
#ifdef _WIN32
        closesocket(listenSocket);
#else
        close(listenSocket);
#endif
        listenSocket = INVALID_SOCKET;
        return false;
    }

    return true;
}

std::string Server::clientAddressToString(sockaddr_in clientAddr) {
    std::ostringstream oss;
    oss << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port);
    return oss.str();
}
