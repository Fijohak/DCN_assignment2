// ClientHandler - Processes a single client connection in its own thread.
// Handles authentication, course queries, and admin CRUD operations.

#include "client_handler.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <sstream>
#include <iomanip>

// Converts raw bytes to a hex string (used by the ENCRYPT demo command).
static std::string bytesToHex(const std::string& data) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < data.size(); ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(static_cast<unsigned char>(data[i]));
    }
    return oss.str();
}

ClientHandler::ClientHandler(SOCKET clientSocket,
                             CourseDB& database,
                             Logger& logger,
                             const std::string& clientAddress,
                             std::function<void()> onDisconnect)
    : clientSocket(clientSocket),
      database(database),
      logger(logger),
      clientAddress(clientAddress),
      loggedIn(false),
      isAdmin(false),
      onDisconnect(onDisconnect) {}

void ClientHandler::operator()() {
    logger.info("Client connected: " + clientAddress + " [Active connections: +1]");
    sendResponse("OK Connected to Course Timetable Server. Use ENCRYPT <text>|<key> to test encryption.\r\n");

    bool shouldClose = false;
    std::string line;
    int cmdCount = 0;

    while (!shouldClose && receiveLine(line)) {
        cmdCount++;
        logger.info(clientAddress + " >> " + line);
        const std::string response = handleCommand(line, shouldClose);
        if (!response.empty()) {
            // Log the response summary (first line only for brevity)
            std::string respSummary = response;
            size_t newlinePos = respSummary.find('\n');
            if (newlinePos != std::string::npos) {
                respSummary = respSummary.substr(0, newlinePos);
            }
            size_t crPos = respSummary.find('\r');
            if (crPos != std::string::npos) {
                respSummary = respSummary.substr(0, crPos);
            }
            logger.info(clientAddress + " << " + respSummary);
            
            if (!sendResponse(response)) {
                break;
            }
        }
    }

#ifdef _WIN32
    closesocket(clientSocket);
#else
    close(clientSocket);
#endif

    if (onDisconnect) {
        onDisconnect();
    }

    logger.info("Client disconnected: " + clientAddress + " [processed " + std::to_string(cmdCount) + " commands]");
}

bool ClientHandler::receiveLine(std::string& line) {
    line.clear();
    char ch = '\0';

    while (true) {
        const int bytesReceived = recv(clientSocket, &ch, 1, 0);
        if (bytesReceived <= 0) {
            return false;
        }

        if (ch == '\n') {
            return true;
        }

        if (ch != '\r') {
            line.push_back(ch);
        }

        if (line.size() > 4096) {
            line.clear();
            return true;
        }
    }
}

bool ClientHandler::sendResponse(const std::string& response) {
    const char* data = response.c_str();
    int remaining = static_cast<int>(response.size());

    while (remaining > 0) {
        const int sent = send(clientSocket, data, remaining, 0);
        if (sent <= 0) {
            return false;
        }

        data += sent;
        remaining -= sent;
    }

    return true;
}

std::string ClientHandler::handleCommand(const std::string& line, bool& shouldClose) {
    const Request request = Protocol::parseRequest(line);

    switch (request.type) {
        case CommandType::Empty:
            return "ERROR Empty command\r\n";

        case CommandType::Ping:
            logger.info(clientAddress + " PING");
            return "OK PONG\r\n";

        case CommandType::Help:
            return Protocol::helpText();

        case CommandType::Quit:
            shouldClose = true;
            return "OK Goodbye\r\n";

        case CommandType::Logout:
            if (!loggedIn) return "ERROR Not logged in\r\n";
            loggedIn = false;
            isAdmin = false;
            logger.info(clientAddress + " LOGOUT: " + username + " logged out");
            username.clear();
            return "OK Logged out successfully\r\n";

        case CommandType::Exit:
            shouldClose = true;
            logger.info(clientAddress + " EXIT: connection closing");
            return "OK Goodbye\r\n";

        // ---- Queries (require login) ----
        case CommandType::ListAll:
            if (!loggedIn) return "ERROR Please login first\r\n";
            logger.info(clientAddress + " LIST_ALL");
            return Protocol::formatCourses(database.getAllCourses());

        case CommandType::QueryCode:
            if (!loggedIn) return "ERROR Please login first\r\n";
            logger.info(clientAddress + " QUERY_CODE " + request.argument);
            return Protocol::formatCourses(database.queryByCourseCode(request.argument));

        case CommandType::QueryInstructor:
            if (!loggedIn) return "ERROR Please login first\r\n";
            logger.info(clientAddress + " QUERY_INSTRUCTOR " + request.argument);
            return Protocol::formatCourses(database.queryByInstructor(request.argument));

        case CommandType::QuerySemester:
            if (!loggedIn) return "ERROR Please login first\r\n";
            logger.info(clientAddress + " QUERY_SEMESTER " + request.argument);
            return Protocol::formatCourses(database.queryBySemester(request.argument));

        // ---- Authentication ----
        case CommandType::Login: {
            if (request.fields.size() < 2)
                return "FAILURE Usage: LOGIN <username> <password>\r\n";

            Auth::LoginResult result = auth.login(request.fields[0], request.fields[1]);
            if (!result.success) {
                logger.error(clientAddress + " login failure: " + request.fields[0]);
                return "FAILURE Invalid username or password\r\n";
            }

            loggedIn = true;
            isAdmin = (result.role == UserRole::Admin);
            username = request.fields[0];
            logger.info(clientAddress + " login: " + username +
                        (isAdmin ? " (Admin)" : " (Student)"));
            return "SUCCESS Login successful. Role: " +
                   std::string(isAdmin ? "Admin" : "Student") + "\r\n";
        }

        case CommandType::Register: {
            if (request.fields.size() < 2)
                return "FAILURE Usage: REGISTER <username> <password>\r\n";
            if (request.fields[0].length() < 3)
                return "FAILURE Username must be at least 3 characters\r\n";
            if (request.fields[1].length() < 4)
                return "FAILURE Password must be at least 4 characters\r\n";

            Auth::LoginResult result = auth.registerUser(request.fields[0], request.fields[1]);
            if (!result.success)
                return "FAILURE Username already exists\r\n";

            // Auto-login after successful registration
            loggedIn = true;
            isAdmin = (result.role == UserRole::Admin);
            username = request.fields[0];
            logger.info(clientAddress + " registered and logged in: " + username +
                        (isAdmin ? " (Admin)" : " (Student)"));
            return "SUCCESS Registration successful. Role: " +
                   std::string(isAdmin ? "Admin" : "Student") + "\r\n";
        }

        // ---- Admin operations ----
        case CommandType::Add: {
            if (!loggedIn || !isAdmin) {
                return "ERROR Permission denied\r\n";
            }

            Course course;
            course.semester = request.fields[0];
            course.courseCode = request.fields[1];
            course.courseTitle = request.fields[2];
            course.section = request.fields[3];
            course.instructor = request.fields[4];
            course.day = request.fields[5];
            course.startTime = request.fields[6];
            course.endTime = request.fields[7];
            course.classroom = request.fields[8];

            if (database.addCourse(course)) {
                logger.info(clientAddress + " ADD " + course.courseCode + " " + course.section);
                return "OK Record added\r\n";
            }

            return "ERROR Record already exists or could not be saved\r\n";
        }

        case CommandType::Update:
            if (!loggedIn || !isAdmin) {
                return "ERROR Permission denied\r\n";
            }

            if (database.updateCourseField(request.fields[0],
                                           request.fields[1],
                                           request.fields[2],
                                           request.fields[3])) {
                logger.info(clientAddress + " UPDATE " + request.fields[0] + " " + request.fields[1]);
                return "OK Record updated\r\n";
            }

            return "ERROR Record not found or invalid field\r\n";

        case CommandType::DeleteCourse:
            if (!loggedIn || !isAdmin) {
                return "ERROR Permission denied\r\n";
            }

            if (database.deleteCourse(request.fields[0], request.fields[1])) {
                logger.info(clientAddress + " DELETE " + request.fields[0] + " " + request.fields[1]);
                return "OK Record deleted\r\n";
            }

            return "ERROR Record not found\r\n";

        // ---- Encryption demo ----
        case CommandType::Encrypt: {
            const std::string& data = request.fields[0];
            const std::string& key = request.fields[1];

            std::string encrypted = Protocol::encryptXor(data, key);
            std::string decrypted = Protocol::decryptXor(encrypted, key);

            std::ostringstream oss;
            oss << "RESULT count=4\r\n"
                << "Original: [" << data << "]\r\n"
                << "Key: [" << key << "]\r\n"
                << "Encrypted (hex): [" << bytesToHex(encrypted) << "]\r\n"
                << "Decrypted: [" << decrypted << "]\r\n"
                << "END\r\n";

            logger.info(clientAddress + " ENCRYPT demo: [" + data + "] -> hex: " + bytesToHex(encrypted));

            return oss.str();
        }

        case CommandType::Invalid:
        default:
            logger.error(clientAddress + " invalid command: " + line);
            return "ERROR Invalid command\r\n";
    }
}
