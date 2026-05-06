#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <ctime>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../include/common.h"
#include "../include/protocol.h"

#pragma comment(lib, "ws2_32.lib")

// ==================== 全局数据 ====================
std::vector<CourseRecord> g_courses;
std::map<std::string, std::pair<std::string, UserRole>> g_users;
std::mutex g_mutex;
int g_port = 8888;
bool g_running = true;

// ==================== 工具函数 ====================
std::string getTimeStr() {
    time_t now = time(0);
    char buf[20];
    struct tm t;
    localtime_s(&t, &now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    return std::string(buf);
}

void log(const std::string& msg) {
    std::cout << "[" << getTimeStr() << "] " << msg << std::endl;
    std::ofstream logFile("data/server.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << "[" << getTimeStr() << "] " << msg << std::endl;
        logFile.close();
    }
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delim)) {
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

// ==================== 缓存系统 ====================
struct CacheEntry {
    std::string result;
    time_t timestamp;
};
std::map<std::string, CacheEntry> g_queryCache;
std::mutex g_cacheMutex;
const int CACHE_TTL = 30; // 缓存有效期 30 秒

void invalidateCache() {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    g_queryCache.clear();
    log("Cache invalidated");
}

std::string getCachedResult(const std::string& key) {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    auto it = g_queryCache.find(key);
    if (it != g_queryCache.end()) {
        time_t now = time(0);
        if (difftime(now, it->second.timestamp) < CACHE_TTL) {
            log("Cache HIT for: " + key);
            return it->second.result;
        } else {
            g_queryCache.erase(it);
        }
    }
    log("Cache MISS for: " + key);
    return "";
}

void setCachedResult(const std::string& key, const std::string& result) {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    CacheEntry entry;
    entry.result = result;
    entry.timestamp = time(0);
    g_queryCache[key] = entry;
}

// ==================== 数据库操作 ====================
void loadDatabase() {
    std::ifstream file("data/timetable.csv");
    if (!file.is_open()) {
        log("WARNING: Cannot open data/timetable.csv, starting with empty database");
        return;
    }
    
    std::string line;
    std::getline(file, line);
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        CourseRecord r;
        std::getline(ss, r.courseCode, ',');
        std::getline(ss, r.courseTitle, ',');
        std::getline(ss, r.section, ',');
        std::getline(ss, r.instructor, ',');
        std::getline(ss, r.time, ',');
        std::getline(ss, r.classroom, ',');
        if (!r.courseCode.empty()) {
            g_courses.push_back(r);
        }
    }
    file.close();
    log("Loaded " + std::to_string(g_courses.size()) + " courses from database");
}

void saveDatabase() {
    std::ofstream file("data/timetable.csv");
    if (!file.is_open()) return;
    file << "CourseCode,CourseTitle,Section,Instructor,Time,Classroom\n";
    for (auto& c : g_courses) {
        file << c.courseCode << "," << c.courseTitle << "," << c.section << ","
             << c.instructor << "," << c.time << "," << c.classroom << "\n";
    }
    file.close();
}

// ==================== 用户管理 ====================
void saveUsers() {
    std::ofstream file("data/users.csv");
    if (!file.is_open()) return;
    file << "Username,Password,Role\n";
    for (auto& u : g_users) {
        std::string roleStr = (u.second.second == ROLE_ADMIN) ? "ADMIN" : "STUDENT";
        file << u.first << "," << u.second.first << "," << roleStr << "\n";
    }
    file.close();
}

void loadUsers() {
    std::ifstream file("data/users.csv");
    if (!file.is_open()) {
        log("WARNING: Cannot open data/users.csv, using default users");
        g_users["student"] = {"student123", ROLE_STUDENT};
        g_users["admin"] = {"admin123", ROLE_ADMIN};
        saveUsers();
        return;
    }
    
    std::string line;
    std::getline(file, line); // skip header
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string username, password, roleStr;
        std::getline(ss, username, ',');
        std::getline(ss, password, ',');
        std::getline(ss, roleStr, ',');
        if (!username.empty()) {
            UserRole role = (roleStr == "ADMIN") ? ROLE_ADMIN : ROLE_STUDENT;
            g_users[username] = {password, role};
        }
    }
    file.close();
    log("Loaded " + std::to_string(g_users.size()) + " users from database");
}

UserRole loginUser(const std::string& username, const std::string& password) {
    auto it = g_users.find(username);
    if (it == g_users.end()) return ROLE_NONE;
    if (it->second.first != password) return ROLE_NONE;
    return it->second.second;
}

bool registerUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(g_mutex);
    // Check if username already exists
    if (g_users.find(username) != g_users.end()) {
        return false;
    }
    // Add new user as Student by default
    g_users[username] = {password, ROLE_STUDENT};
    saveUsers();
    log("New user registered: " + username);
    return true;
}

// ==================== 查询函数 ====================
std::string searchByCode(const std::string& code) {
    std::string result;
    for (auto& c : g_courses) {
        if (c.courseCode == code) {
            result += c.courseCode + " | " + c.courseTitle + " | Sec " + c.section +
                      " | " + c.instructor + " | " + c.time + " | " + c.classroom + "\n";
        }
    }
    return result.empty() ? "No courses found with code: " + code : result;
}

std::string searchByInstructor(const std::string& name) {
    std::string result;
    for (auto& c : g_courses) {
        if (c.instructor.find(name) != std::string::npos) {
            result += c.courseCode + " | " + c.courseTitle + " | Sec " + c.section +
                      " | " + c.instructor + " | " + c.time + " | " + c.classroom + "\n";
        }
    }
    return result.empty() ? "No courses found for instructor: " + name : result;
}

std::string viewAll() {
    std::string result;
    for (auto& c : g_courses) {
        result += c.courseCode + " | " + c.courseTitle + " | Sec " + c.section +
                  " | " + c.instructor + " | " + c.time + " | " + c.classroom + "\n";
    }
    return result.empty() ? "No courses available" : result;
}

// ==================== 高级搜索函数 ====================
std::string searchByTime(const std::string& keyword) {
    std::string result;
    for (auto& c : g_courses) {
        // 支持按星期几搜索 (Mon, Tue, Wed, Thu, Fri)
        // 也支持按时间段搜索 (e.g., "10:00", "14")
        if (c.time.find(keyword) != std::string::npos) {
            result += c.courseCode + " | " + c.courseTitle + " | Sec " + c.section +
                      " | " + c.instructor + " | " + c.time + " | " + c.classroom + "\n";
        }
    }
    return result.empty() ? "No courses found for time: " + keyword : result;
}

std::string searchByTitle(const std::string& keyword) {
    std::string result;
    for (auto& c : g_courses) {
        if (c.courseTitle.find(keyword) != std::string::npos) {
            result += c.courseCode + " | " + c.courseTitle + " | Sec " + c.section +
                      " | " + c.instructor + " | " + c.time + " | " + c.classroom + "\n";
        }
    }
    return result.empty() ? "No courses found with title containing: " + keyword : result;
}

std::string searchByClassroom(const std::string& room) {
    std::string result;
    for (auto& c : g_courses) {
        if (c.classroom.find(room) != std::string::npos) {
            result += c.courseCode + " | " + c.courseTitle + " | Sec " + c.section +
                      " | " + c.instructor + " | " + c.time + " | " + c.classroom + "\n";
        }
    }
    return result.empty() ? "No courses found for classroom: " + room : result;
}

std::string searchAdvanced(const std::string& field, const std::string& op, const std::string& value) {
    std::string result;
    for (auto& c : g_courses) {
        std::string fieldVal;
        std::string fieldLower = field;
        for (auto& ch : fieldLower) ch = tolower(ch);
        
        if (fieldLower == "code") fieldVal = c.courseCode;
        else if (fieldLower == "title") fieldVal = c.courseTitle;
        else if (fieldLower == "section" || fieldLower == "sec") fieldVal = c.section;
        else if (fieldLower == "instructor" || fieldLower == "inst") fieldVal = c.instructor;
        else if (fieldLower == "time") fieldVal = c.time;
        else if (fieldLower == "classroom" || fieldLower == "room") fieldVal = c.classroom;
        else return "Unknown field: " + field + ". Valid: code, title, section, instructor, time, classroom\n";
        
        bool match = false;
        if (op == "=" || op == "eq") {
            match = (fieldVal == value);
        } else if (op == "~=" || op == "contains" || op == "like") {
            match = (fieldVal.find(value) != std::string::npos);
        } else if (op == "!=" || op == "ne") {
            match = (fieldVal != value);
        } else if (op == "^=" || op == "startswith") {
            match = (fieldVal.find(value) == 0);
        } else {
            return "Unknown operator: " + op + ". Valid: =, ~=, !=, ^=\n";
        }
        
        if (match) {
            result += c.courseCode + " | " + c.courseTitle + " | Sec " + c.section +
                      " | " + c.instructor + " | " + c.time + " | " + c.classroom + "\n";
        }
    }
    return result.empty() ? "No matching courses found" : result;
}

// ==================== 客户端处理 ====================
void handleClient(SOCKET clientSock) {
    log("New client connected");
    
    std::string welcome = "Welcome to Course Timetable System\n";
    send(clientSock, welcome.c_str(), welcome.length(), 0);
    
    bool loggedIn = false;
    bool isAdmin = false;
    bool encrypted = false;
    std::string username;
    char buffer[4096];
    
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            log("Client disconnected");
            break;
        }
        
        std::string request = trim(buffer);
        
        // 处理加密协商
        if (!encrypted && request.find("ENCRYPT") == 0) {
            encrypted = true;
            std::string encResp = "ENCRYPT_OK\n";
            send(clientSock, encResp.c_str(), encResp.length(), 0);
            log("Encryption enabled for this session");
            continue;
        }
        
        // 解密加密消息
        if (encrypted && isEncrypted(request)) {
            request = decryptMessage(request);
            log("Decrypted: " + request);
        } else {
            log("Received: " + request);
        }
        
        std::vector<std::string> parts = split(request, ' ');
        std::string cmd = parts.empty() ? "" : parts[0];
        for (auto& c : cmd) c = toupper(c);
        
        std::string response;
        
        if (cmd == "LOGIN") {
            if (parts.size() < 3) {
                response = "FAILURE Usage: LOGIN <username> <password>\n";
            } else {
                UserRole role = loginUser(parts[1], parts[2]);
                if (role == ROLE_NONE) {
                    response = "FAILURE Invalid username or password\n";
                } else {
                    loggedIn = true;
                    username = parts[1];
                    isAdmin = (role == ROLE_ADMIN);
                    response = "SUCCESS Login successful. Role: " + std::string(isAdmin ? "Admin" : "Student") + "\n";
                    log("User logged in: " + username + " (Role: " + (isAdmin ? "Admin" : "Student") + ")");
                }
            }
        }
        else if (cmd == "REGISTER") {
            if (parts.size() < 3) {
                response = "FAILURE Usage: REGISTER <username> <password>\n";
            } else if (parts[1].length() < 3) {
                response = "FAILURE Username must be at least 3 characters\n";
            } else if (parts[2].length() < 4) {
                response = "FAILURE Password must be at least 4 characters\n";
            } else {
                if (registerUser(parts[1], parts[2])) {
                    response = "SUCCESS Registration successful. You can now login.\n";
                } else {
                    response = "FAILURE Username already exists\n";
                }
            }
        }
        else if (cmd == "LOGOUT") {
            if (!loggedIn) response = "FAILURE Not logged in\n";
            else {
                loggedIn = false;
                isAdmin = false;
                username = "";
                response = "SUCCESS Logged out\n";
            }
        }
        else if (cmd == "QUERY") {
            if (!loggedIn) {
                response = "FAILURE Please login first\n";
            } else {
                std::string queryType = parts.size() > 1 ? parts[1] : "ALL";
                for (auto& c : queryType) c = toupper(c);
                
                // 尝试从缓存获取
                std::string cacheKey = request;
                std::string result = getCachedResult(cacheKey);
                
                if (result.empty()) {
                    // 缓存未命中，执行查询
                    if (queryType == "CODE" && parts.size() > 2) {
                        result = searchByCode(parts[2]);
                    } else if (queryType == "INSTRUCTOR" && parts.size() > 2) {
                        std::string name;
                        for (size_t i = 2; i < parts.size(); i++) {
                            if (i > 2) name += " ";
                            name += parts[i];
                        }
                        result = searchByInstructor(name);
                    } else if (queryType == "ALL") {
                        result = viewAll();
                    } else if (queryType == "TIME" && parts.size() > 2) {
                        std::string keyword;
                        for (size_t i = 2; i < parts.size(); i++) {
                            if (i > 2) keyword += " ";
                            keyword += parts[i];
                        }
                        result = searchByTime(keyword);
                    } else if (queryType == "TITLE" && parts.size() > 2) {
                        std::string keyword;
                        for (size_t i = 2; i < parts.size(); i++) {
                            if (i > 2) keyword += " ";
                            keyword += parts[i];
                        }
                        result = searchByTitle(keyword);
                    } else if (queryType == "CLASSROOM" && parts.size() > 2) {
                        std::string room;
                        for (size_t i = 2; i < parts.size(); i++) {
                            if (i > 2) room += " ";
                            room += parts[i];
                        }
                        result = searchByClassroom(room);
                    } else if (queryType == "ADVANCED" && parts.size() >= 5) {
                        result = searchAdvanced(parts[2], parts[3], parts[4]);
                    } else {
                        result = searchByCode(parts.size() > 2 ? parts[2] : parts[1]);
                    }
                    // 存入缓存
                    setCachedResult(cacheKey, result);
                } else {
                    log("Serving from cache: " + cacheKey);
                }
                response = "RESULT\n" + result + "END\n";
            }
        }
        else if (cmd == "ADD") {
            if (!loggedIn || !isAdmin) {
                response = "FAILURE Admin privileges required\n";
            } else if (parts.size() < 7) {
                response = "FAILURE Usage: ADD <code> <title> <section> <instructor> <time> <classroom>\n";
            } else {
                std::lock_guard<std::mutex> lock(g_mutex);
                CourseRecord r;
                r.courseCode = parts[1];
                r.courseTitle = parts[2];
                r.section = parts[3];
                r.instructor = parts[4];
                r.time = parts[5];
                r.classroom = parts[6];
                g_courses.push_back(r);
                saveDatabase();
                invalidateCache();
                response = "SUCCESS Course added\n";
                log("Admin " + username + " added course: " + r.courseCode);
            }
        }
        else if (cmd == "UPDATE") {
            if (!loggedIn || !isAdmin) {
                response = "FAILURE Admin privileges required\n";
            } else if (parts.size() < 5) {
                response = "FAILURE Usage: UPDATE <code> <section> <field> <newvalue>\n";
            } else {
                std::lock_guard<std::mutex> lock(g_mutex);
                bool found = false;
                for (auto& c : g_courses) {
                    if (c.courseCode == parts[1] && c.section == parts[2]) {
                        std::string field = parts[3];
                        for (auto& ch : field) ch = tolower(ch);
                        std::string newVal;
                        for (size_t i = 4; i < parts.size(); i++) {
                            if (i > 4) newVal += " ";
                            newVal += parts[i];
                        }
                        if (field == "title") c.courseTitle = newVal;
                        else if (field == "instructor") c.instructor = newVal;
                        else if (field == "time") c.time = newVal;
                        else if (field == "classroom") c.classroom = newVal;
                        else { response = "FAILURE Unknown field: " + field + "\n"; found = true; break; }
                        found = true;
                        saveDatabase();
                        invalidateCache();
                        response = "SUCCESS Course updated\n";
                        log("Admin " + username + " updated course: " + c.courseCode);
                        break;
                    }
                }
                if (!found) response = "FAILURE Course not found\n";
            }
        }
        else if (cmd == "DELETE") {
            if (!loggedIn || !isAdmin) {
                response = "FAILURE Admin privileges required\n";
            } else if (parts.size() < 3) {
                response = "FAILURE Usage: DELETE <code> <section>\n";
            } else {
                std::lock_guard<std::mutex> lock(g_mutex);
                bool found = false;
                for (size_t i = 0; i < g_courses.size(); i++) {
                    if (g_courses[i].courseCode == parts[1] && g_courses[i].section == parts[2]) {
                        g_courses.erase(g_courses.begin() + i);
                        found = true;
                        saveDatabase();
                        invalidateCache();
                        response = "SUCCESS Course deleted\n";
                        log("Admin " + username + " deleted course: " + parts[1]);
                        break;
                    }
                }
                if (!found) response = "FAILURE Course not found\n";
            }
        }
        else {
            response = "ERROR Unknown command\n";
        }
        
        // 如果启用了加密，加密响应
        if (encrypted) {
            response = encryptMessage(response) + "\n";
        }
        send(clientSock, response.c_str(), response.length(), 0);
    }
    
    closesocket(clientSock);
    log("Client handler finished");
}

// ==================== 主函数 ====================
void printProtocolInfo() {
    std::cout << "\n=== Communication Protocol v" << PROTOCOL_VERSION << " ===" << std::endl;
    std::cout << "Transport: TCP, Encoding: ASCII text" << std::endl;
    std::cout << "--- Commands ---" << std::endl;
    std::cout << "  LOGIN <user> <pass>       - Authenticate" << std::endl;
    std::cout << "  REGISTER <user> <pass>    - Register new user" << std::endl;
    std::cout << "  LOGOUT                    - End session" << std::endl;
    std::cout << "  QUERY CODE <code>         - Search by course code" << std::endl;
    std::cout << "  QUERY INSTRUCTOR <name>   - Search by instructor" << std::endl;
    std::cout << "  QUERY ALL                 - View all courses" << std::endl;
    std::cout << "  QUERY TIME <keyword>      - Search by time (Bonus)" << std::endl;
    std::cout << "  QUERY TITLE <keyword>     - Search by title (Bonus)" << std::endl;
    std::cout << "  QUERY CLASSROOM <room>    - Search by room (Bonus)" << std::endl;
    std::cout << "  QUERY ADVANCED <f> <op> <v> - Advanced search (Bonus)" << std::endl;
    std::cout << "  ADD <code> <title> ...    - Add course (Admin)" << std::endl;
    std::cout << "  UPDATE <code> <sec> ...   - Update course (Admin)" << std::endl;
    std::cout << "  DELETE <code> <sec>       - Delete course (Admin)" << std::endl;
    std::cout << "--- Responses ---" << std::endl;
    std::cout << "  SUCCESS <msg>             - Operation succeeded" << std::endl;
    std::cout << "  FAILURE <msg>             - Operation failed" << std::endl;
    std::cout << "  RESULT\\n<data>\\nEND       - Query result" << std::endl;
    std::cout << "  ERROR <msg>               - Unknown command" << std::endl;
    std::cout << "================================" << std::endl;
}

int main() {
    std::cout << "=== Course Timetable Server ===" << std::endl;
    printProtocolInfo();
    
    loadDatabase();
    loadUsers();
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
    
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(g_port);
    
    if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }
    
    if (listen(listenSock, 10) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }
    
    std::cout << "Server running on port " << g_port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    log("Server started on port " + std::to_string(g_port));
    
    while (g_running) {
        sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);
        SOCKET clientSock = accept(listenSock, (sockaddr*)&clientAddr, &addrLen);
        
        if (clientSock == INVALID_SOCKET) {
            if (g_running) {
                std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            }
            break;
        }
        
        std::thread(handleClient, clientSock).detach();
    }
    
    closesocket(listenSock);
    WSACleanup();
    log("Server stopped");
    return 0;
}
