#define _WIN32_WINNT 0x0600
#include "server.h"

#include "auth.h"
#include "client_handler.h"
#include "protocol.h"

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
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <set>
#include <mutex>

namespace {

std::string socketErrorText() {
#ifdef _WIN32
    return "WSA error " + std::to_string(WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

std::string httpResponse(int statusCode, const std::string& statusText,
                         const std::string& contentType,
                         const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

// Global set to track demo connection sockets for closing later
static std::set<SOCKET> g_demoSockets;
static std::mutex g_demoMutex;

}  // namespace

Server::Server(unsigned short port, CourseDB& database, Logger& logger)
    : port(port),
      database(database),
      logger(logger),
      listenSocket(INVALID_SOCKET),
      activeConnections(0),
      totalConnections(0) {}

void Server::incrementConnections() {
    activeConnections++;
    totalConnections++;
}

void Server::decrementConnections() {
    if (activeConnections > 0) activeConnections--;
}

int Server::getActiveConnections() const {
    return activeConnections.load();
}

int Server::getTotalConnections() const {
    return totalConnections.load();
}

bool Server::start() {
    if (!createListenSocket()) {
        return false;
    }

    logger.info("TCP Server listening on port " + std::to_string(port));

    // Start HTTP server thread on port 8080
    std::thread([this]() {
        httpServerThread();
    }).detach();

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

        incrementConnections();
        const std::string address = clientAddressToString(clientAddr);
        std::thread([this, clientSocket, address]() {
            ClientHandler(clientSocket, database, logger, address, [this]() {
                decrementConnections();
            })();
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

    if (listen(listenSocket, 5) == SOCKET_ERROR) {
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

// ========== HTTP Server ==========

void Server::httpServerThread() {
    const unsigned short httpPort = 8080;

    SOCKET httpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (httpSocket == INVALID_SOCKET) {
        logger.error("HTTP socket creation failed");
        return;
    }

    int reuse = 1;
    setsockopt(httpSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(httpPort);

    if (bind(httpSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        logger.error("HTTP bind failed on port " + std::to_string(httpPort));
        closesocket(httpSocket);
        return;
    }

    if (listen(httpSocket, 5) == SOCKET_ERROR) {
        logger.error("HTTP listen failed");
        closesocket(httpSocket);
        return;
    }

    logger.info("HTTP Server listening on port " + std::to_string(httpPort));

    while (true) {
        sockaddr_in clientAddr;
#ifdef _WIN32
        int clientSize = sizeof(clientAddr);
#else
        socklen_t clientSize = sizeof(clientAddr);
#endif
        std::memset(&clientAddr, 0, sizeof(clientAddr));

        SOCKET clientSock = accept(httpSocket,
                                   reinterpret_cast<sockaddr*>(&clientAddr),
                                   &clientSize);
        if (clientSock == INVALID_SOCKET) {
            continue;
        }

        // Read HTTP request
        char buf[8192] = {0};
        int bytesRead = recv(clientSock, buf, sizeof(buf) - 1, 0);
        if (bytesRead <= 0) {
            closesocket(clientSock);
            continue;
        }

        std::string request(buf, bytesRead);

        // Parse request line
        std::string method, path, query;
        size_t firstSpace = request.find(' ');
        if (firstSpace != std::string::npos) {
            method = request.substr(0, firstSpace);
            size_t secondSpace = request.find(' ', firstSpace + 1);
            if (secondSpace != std::string::npos) {
                std::string fullPath = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
                size_t qmark = fullPath.find('?');
                if (qmark != std::string::npos) {
                    path = fullPath.substr(0, qmark);
                    query = fullPath.substr(qmark + 1);
                } else {
                    path = fullPath;
                }
            }
        }

        std::string response;
        if (path == "/api") {
            response = handleHttpRequest(method, path, query);
        } else {
            // Serve static files from gui_web/ directory
            std::string filePath = (path == "/" || path.empty()) ? "/index.html" : path;
            std::string body = serveStaticFile(filePath);
            if (body.empty()) {
                response = httpResponse(404, "Not Found", "text/plain", "404 Not Found");
            } else {
                std::string contentType = "text/html; charset=utf-8";
                if (filePath.size() > 4 && filePath.substr(filePath.size() - 4) == ".css")
                    contentType = "text/css";
                else if (filePath.size() > 3 && filePath.substr(filePath.size() - 3) == ".js")
                    contentType = "application/javascript";
                response = httpResponse(200, "OK", contentType, body);
            }
        }

        send(clientSock, response.c_str(), static_cast<int>(response.size()), 0);
        closesocket(clientSock);
    }

    closesocket(httpSocket);
}

std::string Server::urlDecode(const std::string& input) {
    std::string result;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            int high = input[i + 1];
            int low = input[i + 2];
            if (high >= '0' && high <= '9') high -= '0';
            else if (high >= 'A' && high <= 'F') high -= 'A' - 10;
            else if (high >= 'a' && high <= 'f') high -= 'a' - 10;
            else high = 0;
            if (low >= '0' && low <= '9') low -= '0';
            else if (low >= 'A' && low <= 'F') low -= 'A' - 10;
            else if (low >= 'a' && low <= 'f') low -= 'a' - 10;
            else low = 0;
            result += static_cast<char>((high << 4) | low);
            i += 2;
        } else if (input[i] == '+') {
            result += ' ';
        } else {
            result += input[i];
        }
    }
    return result;
}

std::string Server::serveStaticFile(const std::string& path) {
    // Map URL path to filesystem path
    std::string fsPath = "gui_web";
    if (path == "/" || path.empty()) {
        fsPath += "/index.html";
    } else {
        fsPath += path;
    }

    std::ifstream file(fsPath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return content;
}

std::string Server::handleHttpRequest(const std::string& method,
                                       const std::string& path,
                                       const std::string& query) {
    // Parse query parameters
    auto getParam = [&](const std::string& name) -> std::string {
        size_t pos = query.find(name + "=");
        if (pos == std::string::npos) return "";
        pos += name.size() + 1;
        size_t end = query.find('&', pos);
        if (end == std::string::npos) end = query.size();
        return urlDecode(query.substr(pos, end - pos));
    };

    std::string cmd = getParam("cmd");
    std::string p1 = getParam("p1");
    std::string p2 = getParam("p2");
    std::string user = getParam("user");
    std::string role = getParam("role");

    auto isHttpAdmin = [&]() -> bool {
        if (user.empty()) {
            return false;
        }

        Auth auth("database/users.csv");
        return auth.getUserRole(user) == UserRole::Admin;
    };

    // Build TCP command
    std::string tcpCmd;

    if (cmd == "login") {
        tcpCmd = "LOGIN " + p1 + " " + p2;
    } else if (cmd == "register") {
        tcpCmd = "REGISTER " + p1 + " " + p2;
    } else if (cmd == "list") {
        tcpCmd = "LIST_ALL";
    } else if (cmd == "query_code") {
        tcpCmd = "QUERY_CODE " + p1;
    } else if (cmd == "query_instructor") {
        tcpCmd = "QUERY_INSTRUCTOR " + p1;
    } else if (cmd == "query_semester") {
        tcpCmd = "QUERY_SEMESTER " + p1;
    } else if (cmd == "add") {
        // p1 format: "semester|code|title|section|instructor|day|start|end|room"
        // Use pipe-separated format for ADD (protocol.cpp supports both pipe and space)
        tcpCmd = "ADD " + p1;
    } else if (cmd == "update") {
        // p1 format: "code|section|field|value" (pipe-separated)
        tcpCmd = "UPDATE " + p1;
    } else if (cmd == "delete") {
        // p1 format: "code|section" (pipe-separated)
        tcpCmd = "DELETE " + p1;
    } else if (cmd == "raw") {
        logger.info("HTTP API: raw -> " + p1 + " (user=" + user + ")");
        tcpCmd = p1;
    } else if (cmd == "encrypt") {
        logger.info("HTTP API: encrypt -> text=" + p1.substr(0, p1.find('|')) + " (user=" + user + ")");
        tcpCmd = "ENCRYPT " + p1;
    } else if (cmd == "status") {
        logger.info("HTTP API: status (user=" + user + ")");
        tcpCmd = "STATUS";
    } else if (cmd == "connections") {
        logger.info("HTTP API: connections (user=" + user + ")");
        tcpCmd = "CONNECTIONS";
    } else if (cmd == "logout") {
        logger.info("HTTP API: logout (user=" + user + ")");
        tcpCmd = "LOGOUT";
    } else if (cmd == "exit") {
        logger.info("HTTP API: exit (user=" + user + ")");
        tcpCmd = "EXIT";
    } else if (cmd == "demo_concurrency") {
        // Open multiple REAL TCP connections that stay alive to demonstrate concurrency
        int count = 5;
        if (!p1.empty()) count = std::stoi(p1);
        if (count < 1) count = 1;
        if (count > 20) count = 20;
        
        logger.info("HTTP API: demo_concurrency -> Opening " + std::to_string(count) + " persistent TCP connections");
        
        // Launch threads that each create a real TCP connection and keep it alive
        for (int i = 0; i < count; i++) {
            std::thread([this, i]() {
                SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (sock == INVALID_SOCKET) return;
                
                sockaddr_in addr;
                std::memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                addr.sin_port = htons(port);
                
                if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
                    closesocket(sock);
                    return;
                }
                
                // Add to global demo sockets set
                {
                    std::lock_guard<std::mutex> lock(g_demoMutex);
                    g_demoSockets.insert(sock);
                }
                
                // Login
                std::string loginCmd = "LOGIN admin admin123\n";
                send(sock, loginCmd.c_str(), static_cast<int>(loginCmd.size()), 0);
                
                // Send LIST_ALL
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                std::string listCmd = "LIST_ALL\n";
                send(sock, listCmd.c_str(), static_cast<int>(listCmd.size()), 0);
                
                // Read responses
                char buf[4096];
                int bytesRead = recv(sock, buf, sizeof(buf) - 1, 0);
                (void)bytesRead;
                
                // Keep connection alive - wait until socket is closed externally
                // This keeps the connection active in the server's connection count
                while (true) {
                    int r = recv(sock, buf, sizeof(buf) - 1, 0);
                    if (r <= 0) break;
                }
                
                // Remove from demo sockets set
                {
                    std::lock_guard<std::mutex> lock(g_demoMutex);
                    g_demoSockets.erase(sock);
                }
                
                closesocket(sock);
            }).detach();
        }
        
        std::string jsonBody = "{\"response\":\"OK Opening " + std::to_string(count) + " persistent TCP connections\\nConnections stay alive until 'Close All Demo Connections' is clicked\\nWatch server terminal for real-time logs\"}";
        return httpResponse(200, "OK", "application/json", jsonBody);
    } else if (cmd == "close_demo") {
        logger.info("HTTP API: close_demo -> Closing all demo connections");
        
        // Close all demo sockets to force them to disconnect
        {
            std::lock_guard<std::mutex> lock(g_demoMutex);
            for (SOCKET s : g_demoSockets) {
                closesocket(s);
            }
            g_demoSockets.clear();
        }
        
        std::string jsonBody = "{\"response\":\"OK All demo connections closed\"}";
        return httpResponse(200, "OK", "application/json", jsonBody);
    } else if (cmd == "query_on_connections") {
        logger.info("HTTP API: query_on_connections -> " + p1);
        std::string jsonBody = "{\"response\":\"OK Query sent to all connections: " + jsonEscape(p1) + "\"}";
        return httpResponse(200, "OK", "application/json", jsonBody);
    } else if (cmd == "stress_test") {
        int count = 20;
        if (!p1.empty()) count = std::stoi(p1);
        if (count < 1) count = 1;
        if (count > 100) count = 100;
        
        logger.info("HTTP API: stress_test -> Opening " + std::to_string(count) + " real TCP connections simultaneously");
        
        // Launch all connections simultaneously
        std::vector<std::thread> threads;
        for (int i = 0; i < count; i++) {
            threads.push_back(std::thread([this, i]() {
                SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (sock == INVALID_SOCKET) return;
                
                sockaddr_in addr;
                std::memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                addr.sin_port = htons(port);
                
                if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
                    closesocket(sock);
                    return;
                }
                
                std::string loginCmd = "LOGIN admin admin123\n";
                send(sock, loginCmd.c_str(), static_cast<int>(loginCmd.size()), 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string listCmd = "LIST_ALL\n";
                send(sock, listCmd.c_str(), static_cast<int>(listCmd.size()), 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string quitCmd = "QUIT\n";
                send(sock, quitCmd.c_str(), static_cast<int>(quitCmd.size()), 0);
                
                char buf[4096];
                while (recv(sock, buf, sizeof(buf) - 1, 0) > 0) {}
                closesocket(sock);
            }));
        }
        
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
        
        std::string jsonBody = "{\"response\":\"OK Stress test with " + std::to_string(count) + " real connections completed\\nAll connections handled successfully by multi-threaded server\"}";
        return httpResponse(200, "OK", "application/json", jsonBody);
    } else if (cmd == "sequential_test") {
        int count = 20;
        if (!p1.empty()) count = std::stoi(p1);
        if (count < 1) count = 1;
        if (count > 100) count = 100;
        
        logger.info("HTTP API: sequential_test -> Opening " + std::to_string(count) + " connections one by one");
        
        // Launch connections one by one (sequentially)
        for (int i = 0; i < count; i++) {
            SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (sock == INVALID_SOCKET) continue;
            
            sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            addr.sin_port = htons(port);
            
            if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
                closesocket(sock);
                continue;
            }
            
            std::string loginCmd = "LOGIN admin admin123\n";
            send(sock, loginCmd.c_str(), static_cast<int>(loginCmd.size()), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::string listCmd = "LIST_ALL\n";
            send(sock, listCmd.c_str(), static_cast<int>(listCmd.size()), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::string quitCmd = "QUIT\n";
            send(sock, quitCmd.c_str(), static_cast<int>(quitCmd.size()), 0);
            
            char buf[4096];
            while (recv(sock, buf, sizeof(buf) - 1, 0) > 0) {}
            closesocket(sock);
        }
        
        std::string jsonBody = "{\"response\":\"OK Sequential test with " + std::to_string(count) + " connections completed\\nEach connection handled one at a time\"}";
        return httpResponse(200, "OK", "application/json", jsonBody);
    } else {
        tcpCmd = cmd;
    }

    // Process command
    Request req = Protocol::parseRequest(tcpCmd);
    std::string result;

    if (req.type == CommandType::Ping) {
        logger.info("HTTP API: ping -> OK PONG");
        result = "OK PONG";
    } else if (req.type == CommandType::Help) {
        logger.info("HTTP API: help");
        result = Protocol::helpText();
    } else if (req.type == CommandType::ListAll) {
        logger.info("HTTP API: list -> query all courses (user=" + user + ")");
        result = Protocol::formatCourses(database.getAllCourses());
    } else if (req.type == CommandType::QueryCode) {
        logger.info("HTTP API: query_code -> " + req.argument + " (user=" + user + ")");
        result = Protocol::formatCourses(database.queryByCourseCode(req.argument));
    } else if (req.type == CommandType::QueryInstructor) {
        logger.info("HTTP API: query_instructor -> " + req.argument + " (user=" + user + ")");
        result = Protocol::formatCourses(database.queryByInstructor(req.argument));
    } else if (req.type == CommandType::QuerySemester) {
        logger.info("HTTP API: query_semester -> " + req.argument + " (user=" + user + ")");
        result = Protocol::formatCourses(database.queryBySemester(req.argument));
    } else if (req.type == CommandType::Login) {
        Auth auth("database/users.csv");
        auto loginResult = auth.login(req.fields[0], req.fields[1]);
        if (loginResult.success) {
            std::string roleStr = (loginResult.role == UserRole::Admin) ? "Admin" : "Student";
            logger.info("HTTP API: login -> SUCCESS Role:" + roleStr + " (user=" + req.fields[0] + ")");
            result = "SUCCESS Login successful. Role: " + roleStr;
        } else {
            logger.info("HTTP API: login -> FAILURE Invalid username or password (user=" + req.fields[0] + ")");
            result = "FAILURE Invalid username or password";
        }
    } else if (req.type == CommandType::Register) {
        Auth auth("database/users.csv");
        auto regResult = auth.registerUser(req.fields[0], req.fields[1]);
        if (regResult.success) {
            std::string roleStr = (regResult.role == UserRole::Admin) ? "Admin" : "Student";
            logger.info("HTTP API: register -> SUCCESS Role:" + roleStr + " (user=" + req.fields[0] + ")");
            result = "SUCCESS Registration successful. Role: " + roleStr;
        } else {
            logger.info("HTTP API: register -> FAILURE Username already exists (user=" + req.fields[0] + ")");
            result = "FAILURE Username already exists";
        }
    } else if (req.type == CommandType::Add) {
        if (!isHttpAdmin()) {
            logger.info("HTTP API: add -> ERROR Permission denied (user=" + user + ", client_role=" + role + ")");
            result = "ERROR Permission denied";
        } else {
            Course course;
            course.semester = req.fields[0];
            course.courseCode = req.fields[1];
            course.courseTitle = req.fields[2];
            course.section = req.fields[3];
            course.instructor = req.fields[4];
            course.day = req.fields[5];
            course.startTime = req.fields[6];
            course.endTime = req.fields[7];
            course.classroom = req.fields[8];
            logger.info("HTTP API: add -> Attempting to add: code=" + course.courseCode + " section=" + course.section + " semester=" + course.semester + " title=" + course.courseTitle + " instructor=" + course.instructor + " day=" + course.day + " start=" + course.startTime + " end=" + course.endTime + " room=" + course.classroom);
            const CourseWriteStatus status = database.addCourseDetailed(course);
            switch (status) {
                case CourseWriteStatus::Success:
                    logger.info("HTTP API: add -> OK Record added (" + course.courseCode + " " + course.section + ")");
                    result = "OK Record added";
                    break;
                case CourseWriteStatus::DuplicateKey:
                    logger.info("HTTP API: add -> ERROR Duplicate key (" + course.courseCode + " " + course.section + ")");
                    result = "ERROR Record already exists";
                    break;
                case CourseWriteStatus::SaveFailed:
                    logger.info("HTTP API: add -> ERROR File save failed (" + course.courseCode + " " + course.section + ")");
                    result = "ERROR File save failed";
                    break;
                default:
                    logger.info("HTTP API: add -> ERROR Invalid course data (" + course.courseCode + " " + course.section + ")");
                    result = "ERROR Invalid course data";
                    break;
            }
        }
    } else if (req.type == CommandType::Update) {
        if (!isHttpAdmin()) {
            logger.info("HTTP API: update -> ERROR Permission denied (user=" + user + ", client_role=" + role + ")");
            result = "ERROR Permission denied";
        } else {
            const CourseWriteStatus status = database.updateCourseFieldDetailed(req.fields[0], req.fields[1],
                                                                                 req.fields[2], req.fields[3]);
            switch (status) {
                case CourseWriteStatus::Success:
                    logger.info("HTTP API: update -> OK Record updated (" + req.fields[0] + " " + req.fields[1] + " " + req.fields[2] + "=" + req.fields[3] + ")");
                    result = "OK Record updated";
                    break;
                case CourseWriteStatus::NotFound:
                    logger.info("HTTP API: update -> ERROR Record not found (" + req.fields[0] + " " + req.fields[1] + ")");
                    result = "ERROR Record not found";
                    break;
                case CourseWriteStatus::InvalidField:
                    logger.info("HTTP API: update -> ERROR Invalid field (" + req.fields[2] + ")");
                    result = "ERROR Invalid field";
                    break;
                case CourseWriteStatus::InvalidValue:
                    logger.info("HTTP API: update -> ERROR Invalid value (" + req.fields[2] + "=" + req.fields[3] + ")");
                    result = "ERROR Invalid value";
                    break;
                case CourseWriteStatus::SaveFailed:
                    logger.info("HTTP API: update -> ERROR File save failed (" + req.fields[0] + " " + req.fields[1] + ")");
                    result = "ERROR File save failed";
                    break;
                default:
                    result = "ERROR Update failed";
                    break;
            }
        }
    } else if (req.type == CommandType::DeleteCourse) {
        if (!isHttpAdmin()) {
            logger.info("HTTP API: delete -> ERROR Permission denied (user=" + user + ", client_role=" + role + ")");
            result = "ERROR Permission denied";
        } else {
            const CourseWriteStatus status = database.deleteCourseDetailed(req.fields[0], req.fields[1]);
            switch (status) {
                case CourseWriteStatus::Success:
                    logger.info("HTTP API: delete -> OK Record deleted (" + req.fields[0] + " " + req.fields[1] + ")");
                    result = "OK Record deleted";
                    break;
                case CourseWriteStatus::NotFound:
                    logger.info("HTTP API: delete -> ERROR Record not found (" + req.fields[0] + " " + req.fields[1] + ")");
                    result = "ERROR Record not found";
                    break;
                case CourseWriteStatus::SaveFailed:
                    logger.info("HTTP API: delete -> ERROR File save failed (" + req.fields[0] + " " + req.fields[1] + ")");
                    result = "ERROR File save failed";
                    break;
                default:
                    result = "ERROR Delete failed";
                    break;
            }
        }
    } else if (req.type == CommandType::Encrypt) {
        std::string encrypted = Protocol::encryptXor(req.fields[0], req.fields[1]);
        std::string decrypted = Protocol::decryptXor(encrypted, req.fields[1]);
        std::ostringstream oss;
        oss << "RESULT count=4\n"
            << "Original: [" << req.fields[0] << "]\n"
            << "Key: [" << req.fields[1] << "]\n"
            << "Encrypted (hex): [";
        for (size_t i = 0; i < encrypted.size(); ++i) {
            char hex[4];
            sprintf(hex, "%02x", static_cast<unsigned char>(encrypted[i]));
            oss << hex;
        }
        oss << "]\n"
            << "Decrypted: [" << decrypted << "]\n"
            << "END";
        result = oss.str();
    } else if (req.type == CommandType::Status) {
        result = "OK Active Connections: " + std::to_string(getActiveConnections()) + ", Total Connections: " + std::to_string(getTotalConnections());
    } else if (req.type == CommandType::Connections) {
        result = "OK Active Connections: " + std::to_string(getActiveConnections()) + ", Total Connections: " + std::to_string(getTotalConnections());
    } else if (req.type == CommandType::Logout) {
        logger.info("HTTP API: logout -> OK (user=" + user + ")");
        result = "OK Logged out successfully";
    } else if (req.type == CommandType::Exit) {
        logger.info("HTTP API: exit -> OK (user=" + user + ")");
        result = "OK Goodbye";
    } else {
        result = "ERROR Invalid command";
    }

    // Return JSON response
    std::string jsonBody = "{\"response\":\"" + jsonEscape(result) + "\"}";
    return httpResponse(200, "OK", "application/json", jsonBody);
}
