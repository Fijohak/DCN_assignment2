#ifndef SERVER_CLIENT_HANDLER_H
#define SERVER_CLIENT_HANDLER_H

#include "../database/course_db.h"
#include "auth.h"
#include "logger.h"
#include "protocol.h"

#ifdef _WIN32
#include <winsock2.h>
#else
typedef int SOCKET;
#endif

#include <string>

class ClientHandler {
public:
    ClientHandler(SOCKET clientSocket,
                  CourseDB& database,
                  Logger& logger,
                  const std::string& clientAddress);

    void operator()();

private:
    SOCKET clientSocket;
    CourseDB& database;
    Logger& logger;
    std::string clientAddress;
    bool loggedIn;
    bool isAdmin;
    std::string username;
    Auth auth{"database/users.csv"};

    bool receiveLine(std::string& line);
    bool sendResponse(const std::string& response);
    std::string handleCommand(const std::string& line, bool& shouldClose);
};

#endif  // SERVER_CLIENT_HANDLER_H
