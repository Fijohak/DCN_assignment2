// Course Timetable System - Console Client
// Usage: timetable_client.exe [--list [ip] [port]]

#include <iostream>
#include <string>
#include <sstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../include/protocol.h"
#include "../include/network_client.h"

// ==================== Helpers ====================

// Strip response prefixes and trailing \r\n for display.
std::string cleanResponse(const std::string& response) {
    std::string display = response;

    // Strip known prefixes
    if (display.find(OK_PREFIX) == 0)
        display = display.substr(strlen(OK_PREFIX));
    else if (display.find(SUCCESS_PREFIX) == 0)
        display = display.substr(strlen(SUCCESS_PREFIX));
    else if (display.find(FAILURE_PREFIX) == 0)
        display = display.substr(strlen(FAILURE_PREFIX));
    else if (display.find(ERROR_PREFIX) == 0)
        display = display.substr(strlen(ERROR_PREFIX));

    // Strip trailing \r\n
    while (!display.empty() && (display.back() == '\n' || display.back() == '\r'))
        display.pop_back();

    return display;
}

// Print a multi-line RESULT response, parsing out the count header and END footer.
void printResult(const std::string& response) {
    std::istringstream stream(response);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line == "END" || line.find("RESULT") == 0)
            continue;
        std::cout << line << '\n';
    }
}

bool isError(const std::string& response) {
    return response.find(ERROR_PREFIX) == 0 ||
           response.find(FAILURE_PREFIX) == 0;
}

// ==================== Winsock ====================

int initializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return result;
    }
    return 0;
}

void cleanupWinsock() {
    WSACleanup();
}

// ==================== --list mode ====================

int quickListCourses(const std::string& ip, int port) {
    NetworkClient client;
    if (!client.connect(ip, port)) {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }

    // Auto-login as student for --list mode
    std::string loginResp = client.sendRequest(
        std::string(CMD_LOGIN) + " student student123");
    if (loginResp.find(SUCCESS_PREFIX) != 0) {
        std::cerr << "Auto-login failed for --list mode" << std::endl;
        client.disconnect();
        return 1;
    }

    std::string response = client.sendRequest(CMD_LIST_ALL);
    client.disconnect();

    if (response.empty()) {
        std::cerr << "No response from server" << std::endl;
        return 1;
    }
    printResult(response);
    return 0;
}

// ==================== Main ====================

int main(int argc, char* argv[]) {
    // Parse --list flag
    bool listMode = false;
    std::string listIp = "127.0.0.1";
    int listPort = 54000;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--list") {
            listMode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') listIp = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') listPort = std::stoi(argv[++i]);
        }
    }

    if (initializeWinsock() != 0) return 1;

    if (listMode) {
        int rc = quickListCourses(listIp, listPort);
        cleanupWinsock();
        return rc;
    }

    // ==================== Interactive mode ====================
    NetworkClient client;
    bool loggedIn = false;
    bool isAdmin = false;

    std::string ip = "127.0.0.1";
    int port = 54000;

    std::cout << "=== Course Timetable Client ===\n";
    std::cout << "Server IP (default: 127.0.0.1): ";
    std::string userInput;
    std::getline(std::cin, userInput);
    if (!userInput.empty()) ip = userInput;

    std::cout << "Port (default: 54000): ";
    std::getline(std::cin, userInput);
    if (!userInput.empty()) {
        try { port = std::stoi(userInput); }
        catch (...) { std::cerr << "Invalid port, using 8888\n"; }
    }

    if (!client.connect(ip, port)) {
        std::cerr << "Connection failed\n";
        cleanupWinsock();
        return 1;
    }

    // Show welcome
    std::string welcome = client.sendRequest("");
    if (!welcome.empty()) {
        std::cout << "\n" << cleanResponse(welcome) << "\n";
    }

    // Main loop
    while (true) {
        if (!client.isConnected()) {
            std::cout << "\nConnection lost. Exiting.\n";
            break;
        }

        if (!loggedIn) {
            std::cout << "\n--- Main Menu ---\n"
                      << "1. Login\n"
                      << "2. Register\n"
                      << "3. Quit\n"
                      << "Choice: ";
            std::string choice;
            std::getline(std::cin, choice);

            if (choice == "1") {
                std::string user, pwd;
                std::cout << "Username: ";
                std::getline(std::cin, user);
                std::cout << "Password: ";
                std::getline(std::cin, pwd);

                std::string response = client.sendRequest(
                    std::string(CMD_LOGIN) + " " + user + " " + pwd);
                if (response.find(SUCCESS_PREFIX) == 0) {
                    loggedIn = true;
                    isAdmin = (response.find("Role: Admin") != std::string::npos);
                    std::cout << "Login successful (" << (isAdmin ? "Admin" : "Student") << ")\n";
                } else {
                    std::cout << "Login failed: " << cleanResponse(response) << "\n";
                }
            } else if (choice == "2") {
                std::string user, pwd;
                std::cout << "New Username: ";
                std::getline(std::cin, user);
                std::cout << "New Password: ";
                std::getline(std::cin, pwd);

                std::string response = client.sendRequest(
                    std::string(CMD_REGISTER) + " " + user + " " + pwd);
                if (response.find(SUCCESS_PREFIX) == 0) {
                    // Auto-login after successful registration
                    loggedIn = true;
                    isAdmin = (response.find("Role: Admin") != std::string::npos);
                    std::cout << "Registration successful. Auto-logged in as "
                              << (isAdmin ? "Admin" : "Student") << "\n";
                } else {
                    std::cout << "Registration failed: " << cleanResponse(response) << "\n";
                }
            } else if (choice == "3") {
                break;
            } else {
                std::cout << "Invalid choice.\n";
            }
        } else {
            // Role-based menu (logged in)
            if (isAdmin) {
                std::cout << "\n--- Admin Menu ---\n"
                          << "1. List All Courses\n"
                          << "2. Search by Course Code\n"
                          << "3. Search by Instructor\n"
                          << "4. Search by Semester\n"
                          << "5. Add Course\n"
                          << "6. Update Course\n"
                          << "7. Delete Course\n"
                          << "8. Logout\n";
            } else {
                std::cout << "\n--- Student Menu ---\n"
                          << "1. List All Courses\n"
                          << "2. Search by Course Code\n"
                          << "3. Search by Instructor\n"
                          << "4. Search by Semester\n"
                          << "5. Logout\n";
            }
            std::cout << "Choice: ";
            std::string choice;
            std::getline(std::cin, choice);

            if (choice == "1") {
                std::string response = client.sendRequest(CMD_LIST_ALL);
                if (isError(response))
                    std::cout << "Error: " << cleanResponse(response) << "\n";
                else
                    printResult(response);
            } else if (choice == "2") {
                std::string code;
                std::cout << "Course Code: ";
                std::getline(std::cin, code);
                std::string response = client.sendRequest(
                    std::string(CMD_QUERY_CODE) + " " + code);
                if (isError(response))
                    std::cout << "Error: " << cleanResponse(response) << "\n";
                else
                    printResult(response);
            } else if (choice == "3") {
                std::string name;
                std::cout << "Instructor Name: ";
                std::getline(std::cin, name);
                std::string response = client.sendRequest(
                    std::string(CMD_QUERY_INSTRUCTOR) + " " + name);
                if (isError(response))
                    std::cout << "Error: " << cleanResponse(response) << "\n";
                else
                    printResult(response);
            } else if (choice == "4") {
                std::string sem;
                std::cout << "Semester (e.g. 2026 Spring): ";
                std::getline(std::cin, sem);
                std::string response = client.sendRequest(
                    std::string(CMD_QUERY_SEMESTER) + " " + sem);
                if (isError(response))
                    std::cout << "Error: " << cleanResponse(response) << "\n";
                else
                    printResult(response);
            } else if (choice == "5" && !isAdmin) {
                // Student logout
                loggedIn = false;
                isAdmin = false;
                std::cout << "Logged out.\n";
            } else if (choice == "5" && isAdmin) {
                // ADD uses 9 pipe-separated fields (admin only)
                std::string semester, code, title, section, instructor;
                std::string day, startTime, endTime, classroom;
                std::cout << "Semester (e.g. 2026 Spring): ";
                std::getline(std::cin, semester);
                std::cout << "Course Code: ";
                std::getline(std::cin, code);
                std::cout << "Title: ";
                std::getline(std::cin, title);
                std::cout << "Section: ";
                std::getline(std::cin, section);
                std::cout << "Instructor: ";
                std::getline(std::cin, instructor);
                std::cout << "Day (e.g. Mon): ";
                std::getline(std::cin, day);
                std::cout << "Start Time (e.g. 10:00): ";
                std::getline(std::cin, startTime);
                std::cout << "End Time (e.g. 12:00): ";
                std::getline(std::cin, endTime);
                std::cout << "Classroom: ";
                std::getline(std::cin, classroom);

                std::string addCmd = std::string(CMD_ADD) + " " +
                    semester + "|" + code + "|" + title + "|" + section + "|" +
                    instructor + "|" + day + "|" + startTime + "|" + endTime + "|" + classroom;
                std::string response = client.sendRequest(addCmd);
                std::cout << cleanResponse(response) << "\n";
            } else if (choice == "6" && isAdmin) {
                // UPDATE uses 4 pipe-separated fields (admin only)
                std::string code, section, field, newValue;
                std::cout << "Course Code: ";
                std::getline(std::cin, code);
                std::cout << "Section: ";
                std::getline(std::cin, section);
                std::cout << "Field (COURSE_TITLE/INSTRUCTOR/TIME/CLASSROOM/DAY/SEMESTER): ";
                std::getline(std::cin, field);
                std::cout << "New Value: ";
                std::getline(std::cin, newValue);

                std::string updCmd = std::string(CMD_UPDATE) + " " +
                    code + "|" + section + "|" + field + "|" + newValue;
                std::string response = client.sendRequest(updCmd);
                std::cout << cleanResponse(response) << "\n";
            } else if (choice == "7" && isAdmin) {
                // DELETE uses 2 pipe-separated fields (admin only)
                std::string code, section;
                std::cout << "Course Code: ";
                std::getline(std::cin, code);
                std::cout << "Section: ";
                std::getline(std::cin, section);

                std::string delCmd = std::string(CMD_DELETE) + " " +
                    code + "|" + section;
                std::string response = client.sendRequest(delCmd);
                std::cout << cleanResponse(response) << "\n";
            } else if (choice == "8" && isAdmin) {
                // Admin logout
                loggedIn = false;
                isAdmin = false;
                std::cout << "Logged out.\n";
            } else {
                std::cout << "Invalid choice.\n";
            }
        }
    }

    client.disconnect();
    cleanupWinsock();
    std::cout << "Goodbye!\n";
    return 0;
}
