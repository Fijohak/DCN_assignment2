#include "../database/course_db.h"
#include "logger.h"
#include "server.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <iostream>

int main() {
    const unsigned short port = 54000;
    Logger logger("logs/server.log");

#ifdef _WIN32
    WSADATA wsaData;
    const int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        std::cerr << "WSAStartup failed: " << wsaResult << std::endl;
        return 1;
    }
#endif

    CourseDB database("database/courses.csv");
    if (!database.loadFromFile()) {
        std::cerr << "Failed to load database/courses.csv" << std::endl;
        logger.error("Failed to load database/courses.csv");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "Course Timetable Server running on port " << port << std::endl;
    std::cout << "Press Ctrl+C to stop the server." << std::endl;
    logger.info("Server starting");

    const unsigned short httpPort = 8080;
    Server server(port, httpPort, database, logger);
    const bool ok = server.start();

#ifdef _WIN32
    WSACleanup();
#endif

    return ok ? 0 : 1;
}
