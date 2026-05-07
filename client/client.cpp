#include <iostream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <map>
#include <ctime>
#include "../include/protocol.h"
#include "../include/common.h"
#include "../include/network_client.h"

// Global Winsock state
WSADATA wsaData;
bool wsaInitialized = false;

std::string escapeInput(const std::string& input) {
    std::string result;
    for (char c : input) {
        if (c == '\r' || c == '\n' || c == '\\') {
            result += '\\';
        }
        result += c;
    }
    return result;
}

enum class Role { STUDENT, ADMIN };

class ClientSession {
private:
    bool m_loggedIn;
    std::string m_username;
    Role m_role;

    struct CacheEntry {
        std::string result;
        time_t timestamp;
    };
    std::map<std::string, CacheEntry> m_cache;
    static const int CACHE_TTL = 15;

public:
    ClientSession() : m_loggedIn(false), m_role(Role::STUDENT) {}

    bool isLoggedIn() const { return m_loggedIn; }
    void login(const std::string& username, Role role) {
        m_loggedIn = true;
        m_username = username;
        m_role = role;
    }
    void logout() {
        m_loggedIn = false;
        m_username.clear();
        m_role = Role::STUDENT;
        clearCache();
    }
    std::string getUsername() const { return m_username; }
    Role getRole() const { return m_role; }

    std::string getCached(const std::string& key) {
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            time_t now = time(0);
            if (difftime(now, it->second.timestamp) < CACHE_TTL) {
                return it->second.result;
            }
            m_cache.erase(it);
        }
        return "";
    }

    void setCache(const std::string& key, const std::string& result) {
        CacheEntry entry;
        entry.result = result;
        entry.timestamp = time(0);
        m_cache[key] = entry;
    }

    void clearCache() { m_cache.clear(); }
};

class CourseBrowser {
private:
    NetworkClient& m_client;
    ClientSession& m_session;

    bool isError(const std::string& response) {
        return response.find(ERROR_PREFIX) == 0 ||
               response.find(FAILURE_PREFIX) == 0;
    }

    void showError(const std::string& response) {
        std::string msg = response;
        if (msg.find(ERROR_PREFIX) == 0)
            msg = msg.substr(strlen(ERROR_PREFIX));
        else if (msg.find(FAILURE_PREFIX) == 0)
            msg = msg.substr(strlen(FAILURE_PREFIX));
        // Trim trailing newline
        if (!msg.empty() && msg.back() == '\n') msg.pop_back();
        std::cout << "Error: " << msg << std::endl;
    }

public:
    CourseBrowser(NetworkClient& client, ClientSession& session)
        : m_client(client), m_session(session) {}

    bool searchByCode(const std::string& code) {
        std::string cacheKey = std::string(CMD_QUERY) + " " + QUERY_CODE + " " + code;
        std::string cached = m_session.getCached(cacheKey);

        if (!cached.empty()) {
            std::cout << cached;
            return true;
        }

        std::string response = m_client.sendRequest(cacheKey);
        if (isError(response)) {
            showError(response);
            return false;
        }
        std::cout << response;
        m_session.setCache(cacheKey, response);
        return true;
    }

    bool searchByInstructor(const std::string& instructor) {
        std::string cacheKey = std::string(CMD_QUERY) + " " + QUERY_INSTRUCTOR + " " + instructor;
        std::string cached = m_session.getCached(cacheKey);

        if (!cached.empty()) {
            std::cout << cached;
            return true;
        }

        std::string response = m_client.sendRequest(cacheKey);
        if (isError(response)) {
            showError(response);
            return false;
        }
        std::cout << response;
        m_session.setCache(cacheKey, response);
        return true;
    }

    bool viewAllCourses() {
        std::string response = m_client.sendRequest(
            std::string(CMD_QUERY) + " " + QUERY_ALL);
        if (isError(response)) {
            showError(response);
            return false;
        }
        std::cout << response;
        return true;
    }

    bool searchByTime(const std::string& keyword) {
        std::string response = m_client.sendRequest(
            std::string(CMD_QUERY) + " " + QUERY_TIME + " " + keyword);
        if (isError(response)) {
            showError(response);
            return false;
        }
        std::cout << response;
        return true;
    }

    bool searchByTitle(const std::string& keyword) {
        std::string response = m_client.sendRequest(
            std::string(CMD_QUERY) + " " + QUERY_TITLE + " " + keyword);
        if (isError(response)) {
            showError(response);
            return false;
        }
        std::cout << response;
        return true;
    }

    bool searchByClassroom(const std::string& room) {
        std::string response = m_client.sendRequest(
            std::string(CMD_QUERY) + " " + QUERY_CLASSROOM + " " + room);
        if (isError(response)) {
            showError(response);
            return false;
        }
        std::cout << response;
        return true;
    }

    bool advancedSearch(const std::string& field, const std::string& op,
                        const std::string& value) {
        std::string response = m_client.sendRequest(
            std::string(CMD_QUERY) + " " + QUERY_ADVANCED + " " +
            field + " " + op + " " + value);
        if (isError(response)) {
            showError(response);
            return false;
        }
        std::cout << response;
        return true;
    }

    bool addCourse(const std::string& code, const std::string& title,
                   const std::string& section, const std::string& instructor,
                   const std::string& time, const std::string& classroom) {
        if (m_session.getRole() != Role::ADMIN) {
            std::cout << "Error: Admin privileges required\n";
            return false;
        }
        std::string response = m_client.sendRequest(
            std::string(CMD_ADD) + " " + escapeInput(code) + " " +
            escapeInput(title) + " " + escapeInput(section) + " " +
            escapeInput(instructor) + " " + escapeInput(time) + " " +
            escapeInput(classroom));
        if (isError(response)) {
            showError(response);
            return false;
        }
        std::cout << response;
        m_session.clearCache();
        return true;
    }

    bool updateCourse(const std::string& code, const std::string& section,
                      const std::string& field, const std::string& newValue) {
        if (m_session.getRole() != Role::ADMIN) {
            std::cout << "Error: Admin privileges required\n";
            return false;
        }
        std::string response = m_client.sendRequest(
            std::string(CMD_UPDATE) + " " + escapeInput(code) + " " +
            escapeInput(section) + " " + escapeInput(field) + " " +
            escapeInput(newValue));
        if (isError(response)) {
            showError(response);
            return false;
        }
        std::cout << response;
        m_session.clearCache();
        return true;
    }

    bool deleteCourse(const std::string& code, const std::string& section) {
        if (m_session.getRole() != Role::ADMIN) {
            std::cout << "Error: Admin privileges required\n";
            return false;
        }
        std::string response = m_client.sendRequest(
            std::string(CMD_DELETE) + " " + escapeInput(code) + " " +
            escapeInput(section));
        if (isError(response)) {
            showError(response);
            return false;
        }
        std::cout << response;
        m_session.clearCache();
        return true;
    }
};

int initializeWinsock() {
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return result;
    }
    wsaInitialized = true;
    return 0;
}

void cleanupWinsock() {
    if (wsaInitialized) {
        WSACleanup();
        wsaInitialized = false;
    }
}

// Quick-list mode: connect, fetch all courses, print, and exit.
// Usage: timetable_client.exe --list [ip] [port]
int quickListCourses(const std::string& ip, int port) {
    NetworkClient client;
    if (!client.connect(ip, port)) {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }

    std::string response = client.sendRequest("LIST_ALL");
    client.disconnect();

    if (response.empty()) {
        std::cerr << "No response from server" << std::endl;
        return 1;
    }

    // Parse the RESULT count=N\r\n...\r\nEND\r\n format
    std::istringstream stream(response);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line == "END" || line.find("RESULT") == 0)
            continue;
        // Replace pipe delimiters with spaced columns for readability
        std::cout << line << '\n';
    }
    return 0;
}

int main(int argc, char* argv[]) {
    // Handle --list flag: non-interactive mode, prints all courses and exits
    bool listMode = false;
    std::string listIp = "127.0.0.1";
    int listPort = 8888;
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

    NetworkClient client;
    ClientSession session;
    CourseBrowser browser(client, session);

    std::string ip = "127.0.0.1";
    int port = 8888;

    std::cout << "=== Course Timetable Client ===\n";
    std::cout << "Server IP (default: 127.0.0.1): ";
    std::string userInput;
    std::getline(std::cin, userInput);
    if (!userInput.empty()) ip = userInput;

    std::cout << "Port (default: 8888): ";
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

    // Display welcome message (empty request returns stored welcome)
    std::string welcome = client.sendRequest("");
    if (!welcome.empty()) {
        std::cout << "\n" << welcome;
    }

    while (true) {
        if (!client.isConnected()) {
            std::cout << "\n*** Connection lost. Reconnecting... ***\n";
            for (int attempt = 0; attempt < 3; ++attempt) {
                std::cout << "Attempt " << (attempt + 1) << "/3...\n";
                if (client.connect(ip, port)) {
                    std::cout << "Reconnected!\n";
                    // Re-login if was logged in
                    session.logout();
                    break;
                }
                std::cout << "Failed. Retrying in 2s...\n";
                Sleep(2000);
            }
            if (!client.isConnected()) {
                std::cout << "Failed to reconnect. Exiting.\n";
                cleanupWinsock();
                return 1;
            }
        }

        if (!session.isLoggedIn()) {
            std::cout << "\n--- Main Menu ---\n"
                      << "1. Login\n2. Register\n3. Exit\nChoice: ";
            std::string choice;
            std::getline(std::cin, choice);

            if (choice == "1") {
                std::string user, pwd;
                std::cout << "Username: ";
                std::getline(std::cin, user);
                std::cout << "Password: ";
                std::getline(std::cin, pwd);

                std::string response = client.sendRequest(
                    std::string(CMD_LOGIN) + " " + escapeInput(user) + " " + escapeInput(pwd));

                if (response.find(SUCCESS_PREFIX) == 0) {
                    Role role = (response.find("Role: Admin") != std::string::npos)
                                    ? Role::ADMIN : Role::STUDENT;
                    session.login(user, role);
                    std::cout << "SUCCESS Login successful. Role: "
                              << (role == Role::ADMIN ? "Admin" : "Student") << "\n";
                    std::cout << "Logged in as "
                              << (role == Role::ADMIN ? "Administrator" : "Student") << "\n";
                } else {
                    std::string msg = response;
                    if (msg.find(FAILURE_PREFIX) == 0)
                        msg = msg.substr(strlen(FAILURE_PREFIX));
                    if (!msg.empty() && msg.back() == '\n') msg.pop_back();
                    std::cout << "Login failed: " << msg << std::endl;
                }
            } else if (choice == "2") {
                std::string user, pwd;
                std::cout << "New Username: ";
                std::getline(std::cin, user);
                std::cout << "New Password: ";
                std::getline(std::cin, pwd);

                std::string response = client.sendRequest(
                    std::string(CMD_REGISTER) + " " + escapeInput(user) + " " + escapeInput(pwd));

                if (response.find(SUCCESS_PREFIX) == 0) {
                    std::cout << response;
                } else {
                    std::string msg = response;
                    if (msg.find(FAILURE_PREFIX) == 0)
                        msg = msg.substr(strlen(FAILURE_PREFIX));
                    if (!msg.empty() && msg.back() == '\n') msg.pop_back();
                    std::cout << "Registration failed: " << msg << std::endl;
                }
            } else if (choice == "3") {
                break;
            } else {
                std::cout << "Invalid choice.\n";
            }
        } else {
            bool isAdmin = (session.getRole() == Role::ADMIN);

            // Display menu
            if (isAdmin) {
                std::cout << "\n--- Admin Menu ---\n";
            } else {
                std::cout << "\n--- Student Menu ---\n";
            }
            std::cout << "1. Search by Course Code\n"
                      << "2. Search by Instructor\n"
                      << "3. View All Courses\n"
                      << "4. Search by Time\n"
                      << "5. Search by Title\n"
                      << "6. Search by Classroom\n"
                      << "7. Advanced Search\n";

            if (isAdmin) {
                std::cout << "8. Add Course\n"
                          << "9. Update Course\n"
                          << "10. Delete Course\n"
                          << "11. Logout\nChoice: ";
            } else {
                std::cout << "8. Logout\nChoice: ";
            }

            std::string choice;
            std::getline(std::cin, choice);

            if (choice == "1") {
                std::string code;
                std::cout << "Course Code: ";
                std::getline(std::cin, code);
                browser.searchByCode(code);
            } else if (choice == "2") {
                std::string instructor;
                std::cout << "Instructor Name: ";
                std::getline(std::cin, instructor);
                browser.searchByInstructor(instructor);
            } else if (choice == "3") {
                browser.viewAllCourses();
            } else if (choice == "4") {
                std::string keyword;
                std::cout << "Time (e.g., Mon, 10:00): ";
                std::getline(std::cin, keyword);
                browser.searchByTime(keyword);
            } else if (choice == "5") {
                std::string keyword;
                std::cout << "Title keyword: ";
                std::getline(std::cin, keyword);
                browser.searchByTitle(keyword);
            } else if (choice == "6") {
                std::string room;
                std::cout << "Classroom: ";
                std::getline(std::cin, room);
                browser.searchByClassroom(room);
            } else if (choice == "7") {
                std::string field, op, value;
                std::cout << "Field (code/title/section/instructor/time/classroom): ";
                std::getline(std::cin, field);
                std::cout << "Operator (= eq / ~= contains / != ne / ^= startswith): ";
                std::getline(std::cin, op);
                std::cout << "Value: ";
                std::getline(std::cin, value);
                browser.advancedSearch(field, op, value);
            } else if (choice == "8") {
                if (isAdmin) {
                    std::string code, title, section, instructor, tim, classroom;
                    std::cout << "Course Code: ";
                    std::getline(std::cin, code);
                    std::cout << "Title: ";
                    std::getline(std::cin, title);
                    std::cout << "Section: ";
                    std::getline(std::cin, section);
                    std::cout << "Instructor: ";
                    std::getline(std::cin, instructor);
                    std::cout << "Time: ";
                    std::getline(std::cin, tim);
                    std::cout << "Classroom: ";
                    std::getline(std::cin, classroom);
                    browser.addCourse(code, title, section, instructor, tim, classroom);
                } else {
                    // Student logout
                    client.sendRequest(std::string(CMD_LOGOUT));
                    session.logout();
                    std::cout << "Logged out successfully.\n";
                }
            } else if (choice == "9" && isAdmin) {
                std::string code, section, field, newValue;
                std::cout << "Course Code: ";
                std::getline(std::cin, code);
                std::cout << "Section: ";
                std::getline(std::cin, section);
                std::cout << "Field to update (title/instructor/time/classroom): ";
                std::getline(std::cin, field);
                std::cout << "New value: ";
                std::getline(std::cin, newValue);
                browser.updateCourse(code, section, field, newValue);
            } else if (choice == "10" && isAdmin) {
                std::string code, section;
                std::cout << "Course Code: ";
                std::getline(std::cin, code);
                std::cout << "Section: ";
                std::getline(std::cin, section);
                browser.deleteCourse(code, section);
            } else if ((choice == "11" && isAdmin) || (choice == "8" && !isAdmin)) {
                // Already handled student logout above
                if (isAdmin) {
                    client.sendRequest(std::string(CMD_LOGOUT));
                    session.logout();
                    std::cout << "Logged out successfully.\n";
                }
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
