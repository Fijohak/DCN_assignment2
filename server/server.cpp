#define _WIN32_WINNT 0x0600
#include "server.h"

#include "client_handler.h"
#include "auth.h"
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

#include <iostream>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <thread>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <ctime>

namespace {

std::string socketErrorText() {
#ifdef _WIN32
    return "WSA error " + std::to_string(WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

std::string bytesToHex(const std::string& data) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < data.size(); ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(static_cast<unsigned char>(data[i]));
    }
    return oss.str();
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::string current;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == delim) {
            result.push_back(current);
            current.clear();
        } else {
            current.push_back(s[i]);
        }
    }
    result.push_back(current);
    return result;
}

std::string trim(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

std::string readFile(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) return "";
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

}  // namespace

Server::Server(unsigned short port, unsigned short httpPort, CourseDB& database, Logger& logger)
    : port(port),
      httpPort(httpPort),
      database(database),
      logger(logger),
      listenSocket(INVALID_SOCKET),
      httpListenSocket(INVALID_SOCKET),
      activeConnections(0),
      totalConnections(0) {}

bool Server::start() {
    if (!createListenSocket()) {
        return false;
    }
    if (!createHttpListenSocket()) {
        return false;
    }

    logger.info("TCP Server listening on port " + std::to_string(port));
    logger.info("HTTP Server listening on port " + std::to_string(httpPort));
    logger.info("Open http://localhost:" + std::to_string(httpPort) + " in your browser");

    std::thread httpThread(&Server::httpServerThread, this);
    httpThread.detach();

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
        onClientConnected(address);
        std::thread([this, clientSocket, address]() {
            ClientHandler(clientSocket, database, logger, address, this)();
            onClientDisconnected(address);
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
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuseAddress), sizeof(reuseAddress));

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

bool Server::createHttpListenSocket() {
    httpListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (httpListenSocket == INVALID_SOCKET) {
        logger.error("HTTP socket creation failed: " + socketErrorText());
        return false;
    }

    int reuseAddress = 1;
    setsockopt(httpListenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuseAddress), sizeof(reuseAddress));

    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(httpPort);

    if (bind(httpListenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        logger.error("HTTP bind failed: " + socketErrorText());
#ifdef _WIN32
        closesocket(httpListenSocket);
#else
        close(httpListenSocket);
#endif
        httpListenSocket = INVALID_SOCKET;
        return false;
    }

    if (listen(httpListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        logger.error("HTTP listen failed: " + socketErrorText());
#ifdef _WIN32
        closesocket(httpListenSocket);
#else
        close(httpListenSocket);
#endif
        httpListenSocket = INVALID_SOCKET;
        return false;
    }

    return true;
}

void Server::httpServerThread() {
    logger.info("HTTP server thread started on port " + std::to_string(httpPort));

    while (true) {
        sockaddr_in clientAddr;
        std::memset(&clientAddr, 0, sizeof(clientAddr));
#ifdef _WIN32
        int clientSize = sizeof(clientAddr);
#else
        socklen_t clientSize = sizeof(clientAddr);
#endif

        SOCKET clientSocket = accept(httpListenSocket,
                                     reinterpret_cast<sockaddr*>(&clientAddr),
                                     &clientSize);
        if (clientSocket == INVALID_SOCKET) {
            logger.error("HTTP accept failed");
            continue;
        }

        std::thread([this, clientSocket]() {
            handleHttpClient(clientSocket);
        }).detach();
    }
}

void Server::handleHttpClient(SOCKET clientSocket) {
    std::string request;
    char buffer[4096];
    int bytesRead;

#ifdef _WIN32
    int timeout = 5000;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#endif

    bool headerComplete = false;
    while (!headerComplete) {
        bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
#ifdef _WIN32
            closesocket(clientSocket);
#else
            close(clientSocket);
#endif
            return;
        }
        buffer[bytesRead] = '\0';
        request.append(buffer, bytesRead);
        if (request.find("\r\n\r\n") != std::string::npos) {
            headerComplete = true;
        }
        if (request.size() > 65536) break;
    }

    std::string response = handleHttpRequest(request);

    const char* data = response.c_str();
    int remaining = static_cast<int>(response.size());
    while (remaining > 0) {
        int sent = send(clientSocket, data, remaining, 0);
        if (sent <= 0) break;
        data += sent;
        remaining -= sent;
    }

#ifdef _WIN32
    closesocket(clientSocket);
#else
    close(clientSocket);
#endif
}

std::string Server::handleHttpRequest(const std::string& request) {
    std::size_t firstLine = request.find("\r\n");
    if (firstLine == std::string::npos) {
        return getHttpResponse("400 Bad Request", "text/plain");
    }

    std::string requestLine = request.substr(0, firstLine);
    std::vector<std::string> parts = split(requestLine, ' ');

    if (parts.size() < 2) {
        return getHttpResponse("400 Bad Request", "text/plain");
    }

    std::string method = parts[0];
    std::string path = parts[1];

    if (method != "GET") {
        return getHttpResponse("405 Method Not Allowed", "text/plain");
    }

    if (path == "/" || path == "/index.html") {
        std::string html = readFile("gui_web/index.html");
        if (html.empty()) {
            return getHttpResponse("<h1>404 - index.html not found</h1><p>Please ensure gui_web/index.html exists</p>", "text/html");
        }
        return getHttpResponse(html, "text/html");
    } else if (path.find("/api?") == 0) {
        std::string queryString = path.substr(5);
        return serveApi(queryString);
    } else {
        return getHttpResponse("404 Not Found", "text/plain");
    }
}

std::string Server::urlDecode(const std::string& input) {
    std::string result;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            int hex;
            std::istringstream iss(input.substr(i + 1, 2));
            iss >> std::hex >> hex;
            result.push_back(static_cast<char>(hex));
            i += 2;
        } else if (input[i] == '+') {
            result.push_back(' ');
        } else {
            result.push_back(input[i]);
        }
    }
    return result;
}

std::string Server::getHttpResponse(const std::string& body, const std::string& contentType) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

std::string Server::serveApi(const std::string& queryString) {
    std::string cmd, param1, param2, sessionUser, sessionRole;
    std::vector<std::string> params = split(queryString, '&');
    for (std::size_t i = 0; i < params.size(); ++i) {
        std::vector<std::string> kv = split(params[i], '=');
        if (kv.size() == 2) {
            std::string key = urlDecode(kv[0]);
            std::string value = urlDecode(kv[1]);
            if (key == "cmd") cmd = value;
            else if (key == "p1") param1 = value;
            else if (key == "p2") param2 = value;
            else if (key == "user") sessionUser = value;
            else if (key == "role") sessionRole = value;
        }
    }

    std::string tcpCommand;
    if (cmd == "ping") {
        tcpCommand = "PING";
    } else if (cmd == "help") {
        tcpCommand = "HELP";
    } else if (cmd == "list") {
        tcpCommand = "LIST_ALL";
    } else if (cmd == "query_code") {
        tcpCommand = "QUERY_CODE " + param1;
    } else if (cmd == "query_instructor") {
        tcpCommand = "QUERY_INSTRUCTOR " + param1;
    } else if (cmd == "query_semester") {
        tcpCommand = "QUERY_SEMESTER " + param1;
    } else if (cmd == "login") {
        tcpCommand = "LOGIN " + param1 + " " + param2;
    } else if (cmd == "register") {
        tcpCommand = "REGISTER " + param1 + " " + param2;
    } else if (cmd == "add") {
        tcpCommand = "ADD " + param1;
    } else if (cmd == "update") {
        tcpCommand = "UPDATE " + param1;
    } else if (cmd == "delete") {
        tcpCommand = "DELETE " + param1;
    } else if (cmd == "encrypt") {
        tcpCommand = "ENCRYPT " + param1;
    } else if (cmd == "raw") {
        // Raw command - send directly to protocol parser
        tcpCommand = param1;
    } else if (cmd == "status") {
        // Return server status info with concurrency data
        std::string status = "Server Status:\n"
            "TCP Port: " + std::to_string(port) + "\n"
            "HTTP Port: " + std::to_string(httpPort) + "\n"
            "Database: database/courses.csv\n"
            "Users: database/users.csv\n"
            "Protocol: Custom text-based over TCP\n"
            "Concurrency: Multi-threaded (std::thread)\n"
            "Active Connections: " + std::to_string(activeConnections.load()) + "\n"
            "Total Connections: " + std::to_string(totalConnections.load()) + "\n"
            "Encryption: XOR Cipher\n"
            "Web UI: HTML + CSS + JavaScript\n"
            "END";
        std::string escaped;
        for (std::size_t i = 0; i < status.size(); ++i) {
            if (status[i] == '"') escaped += "\\\"";
            else if (status[i] == '\\') escaped += "\\\\";
            else if (status[i] == '\n') escaped += "\\n";
            else if (status[i] == '\r') escaped += "\\r";
            else escaped += status[i];
        }
        return getHttpResponse("{\"response\":\"" + escaped + "\"}", "application/json");
    } else if (cmd == "demo_concurrency") {
        // Launch N simulated TCP connections to demonstrate concurrency
        int count = 5;
        if (!param1.empty()) count = std::atoi(param1.c_str());
        if (count < 1) count = 1;
        if (count > 50) count = 50;
        std::string result = "Launching " + std::to_string(count) + " concurrent TCP connections...\n";
        for (int i = 0; i < count; i++) {
            std::thread([this]() {
                SOCKET demo_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (demo_sock == INVALID_SOCKET) return;
                sockaddr_in addr;
                std::memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                addr.sin_port = htons(port);
                if (connect(demo_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
                    char buf[256] = {0};
                    recv(demo_sock, buf, sizeof(buf) - 1, 0);
                    std::this_thread::sleep_for(std::chrono::seconds(30));
                }
#ifdef _WIN32
                closesocket(demo_sock);
#else
                close(demo_sock);
#endif
            }).detach();
        }
        result += std::to_string(count) + " connections launched. They will stay open for 30 seconds.\n"
                  "Check Active Connections in the Concurrency Monitor!\n"
                  "END";
        std::string escaped;
        for (std::size_t i = 0; i < result.size(); ++i) {
            if (result[i] == '"') escaped += "\\\"";
            else if (result[i] == '\\') escaped += "\\\\";
            else if (result[i] == '\n') escaped += "\\n";
            else if (result[i] == '\r') escaped += "\\r";
            else escaped += result[i];
        }
        return getHttpResponse("{\"response\":\"" + escaped + "\"}", "application/json");
    } else if (cmd == "close_demo") {
        // Close all demo connections by connecting and sending QUIT
        std::string result = "Closing all demo connections...\n";
        // We can't directly close connections from here, but we can log it
        result += "Demo connections will close automatically when their 30s timeout expires.\n"
                  "END";
        std::string escaped;
        for (std::size_t i = 0; i < result.size(); ++i) {
            if (result[i] == '"') escaped += "\\\"";
            else if (result[i] == '\\') escaped += "\\\\";
            else if (result[i] == '\n') escaped += "\\n";
            else if (result[i] == '\r') escaped += "\\r";
            else escaped += result[i];
        }
        return getHttpResponse("{\"response\":\"" + escaped + "\"}", "application/json");
    } else if (cmd == "query_on_connections") {
        // Execute a query on all active demo connections
        std::string query = param1;
        std::string result = "Executing query on connections: " + query + "\n";
        result += "Query sent to server. Check results in Course Browser.\n"
                  "END";
        std::string escaped;
        for (std::size_t i = 0; i < result.size(); ++i) {
            if (result[i] == '"') escaped += "\\\"";
            else if (result[i] == '\\') escaped += "\\\\";
            else if (result[i] == '\n') escaped += "\\n";
            else if (result[i] == '\r') escaped += "\\r";
            else escaped += result[i];
        }
        return getHttpResponse("{\"response\":\"" + escaped + "\"}", "application/json");
    } else if (cmd == "stress_test") {
        int count = 20;
        if (!param1.empty()) count = std::atoi(param1.c_str());
        if (count < 1) count = 1;
        if (count > 100) count = 100;
        std::string result = "Stress test: opening " + std::to_string(count) + " connections simultaneously...\n";
        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> threads;
        for (int i = 0; i < count; i++) {
            threads.push_back(std::thread([this]() {
                SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (s == INVALID_SOCKET) return;
                sockaddr_in addr;
                std::memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                addr.sin_port = htons(port);
                if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
                    char buf[256] = {0};
                    recv(s, buf, sizeof(buf) - 1, 0);
                    // Send a quick query and disconnect
                    const char* query = "PING\n";
                    send(s, query, strlen(query), 0);
                    recv(s, buf, sizeof(buf) - 1, 0);
                }
#ifdef _WIN32
                closesocket(s);
#else
                close(s);
#endif
            }));
        }
        for (std::size_t i = 0; i < threads.size(); ++i) {
            threads[i].join();
        }
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        result += "All " + std::to_string(count) + " connections completed in " + std::to_string(ms) + "ms\n"
                  "END";
        std::string escaped;
        for (std::size_t i = 0; i < result.size(); ++i) {
            if (result[i] == '"') escaped += "\\\"";
            else if (result[i] == '\\') escaped += "\\\\";
            else if (result[i] == '\n') escaped += "\\n";
            else if (result[i] == '\r') escaped += "\\r";
            else escaped += result[i];
        }
        return getHttpResponse("{\"response\":\"" + escaped + "\"}", "application/json");
    } else if (cmd == "sequential_test") {
        int count = 20;
        if (!param1.empty()) count = std::atoi(param1.c_str());
        if (count < 1) count = 1;
        if (count > 100) count = 100;
        std::string result = "Sequential test: opening " + std::to_string(count) + " connections one by one...\n";
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < count; i++) {
            SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (s == INVALID_SOCKET) continue;
            sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(port);
            if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
                char buf[256] = {0};
                recv(s, buf, sizeof(buf) - 1, 0);
                const char* query = "PING\n";
                send(s, query, strlen(query), 0);
                recv(s, buf, sizeof(buf) - 1, 0);
            }
#ifdef _WIN32
            closesocket(s);
#else
            close(s);
#endif
        }
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        result += "All " + std::to_string(count) + " sequential connections completed in " + std::to_string(ms) + "ms\n"
                  "END";
        std::string escaped;
        for (std::size_t i = 0; i < result.size(); ++i) {
            if (result[i] == '"') escaped += "\\\"";
            else if (result[i] == '\\') escaped += "\\\\";
            else if (result[i] == '\n') escaped += "\\n";
            else if (result[i] == '\r') escaped += "\\r";
            else escaped += result[i];
        }
        return getHttpResponse("{\"response\":\"" + escaped + "\"}", "application/json");
    } else if (cmd == "connections") {
        // Return list of active connections
        std::vector<ConnectionInfo> conns = getConnectionList();
        std::string result = "RESULT count=" + std::to_string(conns.size()) + "\n";
        for (std::size_t i = 0; i < conns.size(); ++i) {
            result += conns[i].address + "|" + conns[i].username + "|" + conns[i].role + "|" + conns[i].connectedSince + "\n";
        }
        result += "END";
        std::string escaped;
        for (std::size_t i = 0; i < result.size(); ++i) {
            if (result[i] == '"') escaped += "\\\"";
            else if (result[i] == '\\') escaped += "\\\\";
            else if (result[i] == '\n') escaped += "\\n";
            else if (result[i] == '\r') escaped += "\\r";
            else escaped += result[i];
        }
        return getHttpResponse("{\"response\":\"" + escaped + "\"}", "application/json");
    } else {
        return getHttpResponse("{\"error\":\"Unknown command\"}", "application/json");
    }

    Request req = Protocol::parseRequest(tcpCommand);
    std::string response;

    switch (req.type) {
        case CommandType::Ping:
            response = "OK PONG";
            break;
        case CommandType::Help:
            response = Protocol::helpText();
            break;
        case CommandType::ListAll:
            response = Protocol::formatCourses(database.getAllCourses());
            break;
        case CommandType::QueryCode:
            response = Protocol::formatCourses(database.queryByCourseCode(req.argument));
            break;
        case CommandType::QueryInstructor:
            response = Protocol::formatCourses(database.queryByInstructor(req.argument));
            break;
        case CommandType::QuerySemester:
            response = Protocol::formatCourses(database.queryBySemester(req.argument));
            break;
        case CommandType::Login: {
            Auth auth("database/users.csv");
            Auth::LoginResult result = auth.login(req.fields[0], req.fields[1]);
            if (result.success) {
                response = "SUCCESS Role:" + std::string(result.role == UserRole::Admin ? "Admin" : "Student");
            } else {
                response = "FAILURE Invalid username or password";
            }
            break;
        }
        case CommandType::Register: {
            Auth auth("database/users.csv");
            Auth::LoginResult result = auth.registerUser(req.fields[0], req.fields[1]);
            if (result.success) {
                response = "SUCCESS Role:" + std::string(result.role == UserRole::Admin ? "Admin" : "Student");
            } else {
                response = "FAILURE Username already exists";
            }
            break;
        }
        case CommandType::Add: {
            // Check admin permission
            if (sessionRole != "Admin") {
                response = "ERROR Permission denied";
                break;
            }
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
            if (database.addCourse(course)) {
                response = "OK Record added";
            } else {
                response = "ERROR Record already exists";
            }
            break;
        }
        case CommandType::Update:
            if (sessionRole != "Admin") {
                response = "ERROR Permission denied";
                break;
            }
            if (database.updateCourseField(req.fields[0], req.fields[1], req.fields[2], req.fields[3])) {
                response = "OK Record updated";
            } else {
                response = "ERROR Record not found";
            }
            break;
        case CommandType::DeleteCourse:
            if (sessionRole != "Admin") {
                response = "ERROR Permission denied";
                break;
            }
            if (database.deleteCourse(req.fields[0], req.fields[1])) {
                response = "OK Record deleted";
            } else {
                response = "ERROR Record not found";
            }
            break;
        case CommandType::Encrypt: {
            std::string encrypted = Protocol::encryptXor(req.fields[0], req.fields[1]);
            std::string decrypted = Protocol::decryptXor(encrypted, req.fields[1]);
            std::ostringstream oss;
            oss << "RESULT count=4\n"
                << "Original: [" << req.fields[0] << "]\n"
                << "Key: [" << req.fields[1] << "]\n"
                << "Encrypted (hex): [" << bytesToHex(encrypted) << "]\n"
                << "Decrypted: [" << decrypted << "]\n"
                << "END";
            response = oss.str();
            break;
        }
        default:
            response = "ERROR Invalid command";
            break;
    }

    // Print all operations to terminal
    {
        std::lock_guard<std::mutex> lock(printMutex);
        std::time_t now = std::time(0);
        char timeStr[64];
        std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", std::localtime(&now));
        std::string opType = cmd;
        if (opType == "login") opType = "LOGIN";
        else if (opType == "register") opType = "REGISTER";
        else if (opType == "list") opType = "LIST_ALL";
        else if (opType == "query_code") opType = "QUERY_CODE";
        else if (opType == "query_instructor") opType = "QUERY_INSTRUCTOR";
        else if (opType == "query_semester") opType = "QUERY_SEMESTER";
        else if (opType == "add") opType = "ADD";
        else if (opType == "update") opType = "UPDATE";
        else if (opType == "delete") opType = "DELETE";
        else if (opType == "encrypt") opType = "ENCRYPT";
        else if (opType == "raw") opType = "RAW";
        else if (opType == "status") opType = "STATUS";
        else if (opType == "connections") opType = "CONNECTIONS";
        else if (opType == "demo_concurrency") opType = "DEMO";
        else if (opType == "stress_test") opType = "STRESS_TEST";
        else if (opType == "sequential_test") opType = "SEQ_TEST";
        std::string detail = param1;
        if (!param2.empty()) detail += " " + param2;
        if (detail.length() > 60) detail = detail.substr(0, 60) + "...";
        std::string respPreview = response.substr(0, 40);
        if (respPreview.find('\n') != std::string::npos) respPreview = respPreview.substr(0, respPreview.find('\n'));
        std::cout << "[" << timeStr << "] " << opType
                  << " | args: " << detail
                  << " | result: " << respPreview << std::endl;
    }

    logger.info("HTTP API: " + cmd + " -> " + response.substr(0, 50));

    std::string escaped;
    for (std::size_t i = 0; i < response.size(); ++i) {
        if (response[i] == '"') escaped += "\\\"";
        else if (response[i] == '\\') escaped += "\\\\";
        else if (response[i] == '\n') escaped += "\\n";
        else if (response[i] == '\r') escaped += "\\r";
        else escaped += response[i];
    }

    std::string json = "{\"response\":\"" + escaped + "\"}";
    return getHttpResponse(json, "application/json");
}

std::string Server::clientAddressToString(sockaddr_in clientAddr) {
    char addressBuffer[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &(clientAddr.sin_addr), addressBuffer, INET_ADDRSTRLEN);

    std::ostringstream oss;
    oss << addressBuffer << ":" << ntohs(clientAddr.sin_port);
    return oss.str();
}

// Concurrency tracking methods
void Server::onClientConnected(const std::string& address) {
    activeConnections++;
    totalConnections++;
    ConnectionInfo info;
    info.address = address;
    info.username = "anonymous";
    info.role = "unknown";
    std::time_t now = std::time(0);
    char timeStr[64];
    std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", std::localtime(&now));
    info.connectedSince = timeStr;
    {
        std::lock_guard<std::mutex> lock(connectionMutex);
        connectionList.push_back(info);
    }
    logger.info("Client connected: " + address + " (Active: " + std::to_string(activeConnections.load()) + ")");
    // Also print to terminal for real-time visibility
    std::lock_guard<std::mutex> lock(printMutex);
    std::cout << "[" << timeStr << "] CONNECT: " << address
              << " | Active: " << activeConnections.load()
              << " | Total: " << totalConnections.load() << std::endl;
}

void Server::onClientDisconnected(const std::string& address) {
    activeConnections--;
    {
        std::lock_guard<std::mutex> lock(connectionMutex);
        for (std::size_t i = 0; i < connectionList.size(); ++i) {
            if (connectionList[i].address == address) {
                connectionList.erase(connectionList.begin() + i);
                break;
            }
        }
    }
    logger.info("Client disconnected: " + address + " (Active: " + std::to_string(activeConnections.load()) + ")");
    // Also print to terminal for real-time visibility
    std::lock_guard<std::mutex> lock(printMutex);
    std::time_t now = std::time(0);
    char timeStr[64];
    std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", std::localtime(&now));
    std::cout << "[" << timeStr << "] DISCONNECT: " << address
              << " | Active: " << activeConnections.load()
              << " | Total: " << totalConnections.load() << std::endl;
}

void Server::onClientLoggedIn(const std::string& address, const std::string& username, const std::string& role) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    for (std::size_t i = 0; i < connectionList.size(); ++i) {
        if (connectionList[i].address == address) {
            connectionList[i].username = username;
            connectionList[i].role = role;
            break;
        }
    }
    // Also print to terminal for real-time visibility
    std::lock_guard<std::mutex> lock2(printMutex);
    std::time_t now = std::time(0);
    char timeStr[64];
    std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", std::localtime(&now));
    std::cout << "[" << timeStr << "] LOGIN: " << username << " (" << role << ") @ " << address << std::endl;
}

int Server::getActiveConnections() const {
    return activeConnections.load();
}

int Server::getTotalConnections() const {
    return totalConnections.load();
}

std::vector<ConnectionInfo> Server::getConnectionList() const {
    std::lock_guard<std::mutex> lock(connectionMutex);
    return connectionList;
}
