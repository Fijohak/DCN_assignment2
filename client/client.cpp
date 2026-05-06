#include <iostream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

SOCKET g_sock = INVALID_SOCKET;

// ==================== 网络通信 ====================
bool connectToServer(const std::string& ip, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
    
    g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return false;
    }
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
    
    if (connect(g_sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed" << std::endl;
        closesocket(g_sock);
        WSACleanup();
        return false;
    }
    
    return true;
}

void disconnect() {
    if (g_sock != INVALID_SOCKET) {
        closesocket(g_sock);
    }
    WSACleanup();
}

std::string sendRequest(const std::string& request) {
    std::string data = request + "\n";
    send(g_sock, data.c_str(), data.length(), 0);
    
    std::string result;
    char buffer[4096];
    
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(g_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        
        result += buffer;
        
        if (result.find("END\n") != std::string::npos) break;
        if (result.back() == '\n' && (result.find("SUCCESS") == 0 || 
            result.find("FAILURE") == 0 || result.find("ERROR") == 0)) break;
    }
    
    return result;
}

void displayResults(const std::string& response) {
    std::string display = response;
    
    size_t pos = display.find("RESULT\n");
    if (pos != std::string::npos) display = display.substr(pos + 7);
    
    pos = display.find("END\n");
    if (pos != std::string::npos) display = display.substr(0, pos);
    
    if (display.find("SUCCESS") == 0) display = display.substr(8);
    if (display.find("FAILURE") == 0) display = display.substr(8);
    if (display.find("ERROR") == 0) display = display.substr(6);
    
    std::cout << "\n" << display << std::endl;
}

int main() {
    std::string ip;
    int port;
    
    std::cout << "=== Course Timetable Client ===" << std::endl;
    std::cout << "Server IP (default: 127.0.0.1): ";
    std::getline(std::cin, ip);
    if (ip.empty()) ip = "127.0.0.1";
    
    std::string portStr;
    std::cout << "Port (default: 8888): ";
    std::getline(std::cin, portStr);
    port = portStr.empty() ? 8888 : std::stoi(portStr);
    
    if (!connectToServer(ip, port)) {
        std::cerr << "Failed to connect to server" << std::endl;
        system("pause");
        return 1;
    }
    
    char welcome[1024] = {0};
    recv(g_sock, welcome, sizeof(welcome) - 1, 0);
    std::cout << "\n" << welcome << std::endl;
    
    bool loggedIn = false;
    bool isAdmin = false;
    
    while (true) {
        if (!loggedIn) {
            std::cout << "\n--- Main Menu ---" << std::endl;
            std::cout << "1. Login" << std::endl;
            std::cout << "2. Register" << std::endl;
            std::cout << "3. Exit" << std::endl;
            std::cout << "Choice: ";
            
            std::string choice;
            std::getline(std::cin, choice);
            
            if (choice == "1") {
                std::string username, password;
                std::cout << "Username: ";
                std::getline(std::cin, username);
                std::cout << "Password: ";
                std::getline(std::cin, password);
                
                std::string response = sendRequest("LOGIN " + username + " " + password);
                std::cout << response;
                
                if (response.find("SUCCESS") == 0) {
                    loggedIn = true;
                    isAdmin = (response.find("Admin") != std::string::npos);
                    std::cout << "Logged in as " << (isAdmin ? "Administrator" : "Student") << std::endl;
                }
            } else if (choice == "2") {
                std::string username, password;
                std::cout << "Username: ";
                std::getline(std::cin, username);
                std::cout << "Password: ";
                std::getline(std::cin, password);
                
                std::string response = sendRequest("REGISTER " + username + " " + password);
                std::cout << response;
            } else if (choice == "3") {
                break;
            } else {
                std::cout << "Invalid choice" << std::endl;
            }
        } else if (isAdmin) {
            std::cout << "\n--- Admin Menu ---" << std::endl;
            std::cout << "1. Search by Course Code" << std::endl;
            std::cout << "2. Search by Instructor" << std::endl;
            std::cout << "3. View All Courses" << std::endl;
            std::cout << "4. Add Course" << std::endl;
            std::cout << "5. Update Course" << std::endl;
            std::cout << "6. Delete Course" << std::endl;
            std::cout << "7. Logout" << std::endl;
            std::cout << "Choice: ";
            
            std::string choice;
            std::getline(std::cin, choice);
            
            if (choice == "1") {
                std::string code;
                std::cout << "Course Code: ";
                std::getline(std::cin, code);
                displayResults(sendRequest("QUERY CODE " + code));
            } else if (choice == "2") {
                std::string name;
                std::cout << "Instructor: ";
                std::getline(std::cin, name);
                displayResults(sendRequest("QUERY INSTRUCTOR " + name));
            } else if (choice == "3") {
                displayResults(sendRequest("QUERY ALL"));
            } else if (choice == "4") {
                std::string code, title, section, instructor, time, classroom;
                std::cout << "Course Code: "; std::getline(std::cin, code);
                std::cout << "Title: "; std::getline(std::cin, title);
                std::cout << "Section: "; std::getline(std::cin, section);
                std::cout << "Instructor: "; std::getline(std::cin, instructor);
                std::cout << "Time: "; std::getline(std::cin, time);
                std::cout << "Classroom: "; std::getline(std::cin, classroom);
                std::cout << sendRequest("ADD " + code + " " + title + " " + section + " " + instructor + " " + time + " " + classroom);
            } else if (choice == "5") {
                std::string code, section, field, value;
                std::cout << "Course Code: "; std::getline(std::cin, code);
                std::cout << "Section: "; std::getline(std::cin, section);
                std::cout << "Field (title/instructor/time/classroom): "; std::getline(std::cin, field);
                std::cout << "New Value: "; std::getline(std::cin, value);
                std::cout << sendRequest("UPDATE " + code + " " + section + " " + field + " " + value);
            } else if (choice == "6") {
                std::string code, section;
                std::cout << "Course Code: "; std::getline(std::cin, code);
                std::cout << "Section: "; std::getline(std::cin, section);
                std::cout << sendRequest("DELETE " + code + " " + section);
            } else if (choice == "7") {
                std::cout << sendRequest("LOGOUT");
                loggedIn = false;
                isAdmin = false;
            } else {
                std::cout << "Invalid choice" << std::endl;
            }
        } else {
            std::cout << "\n--- Student Menu ---" << std::endl;
            std::cout << "1. Search by Course Code" << std::endl;
            std::cout << "2. Search by Instructor" << std::endl;
            std::cout << "3. View All Courses" << std::endl;
            std::cout << "4. Logout" << std::endl;
            std::cout << "Choice: ";
            
            std::string choice;
            std::getline(std::cin, choice);
            
            if (choice == "1") {
                std::string code;
                std::cout << "Course Code: ";
                std::getline(std::cin, code);
                displayResults(sendRequest("QUERY CODE " + code));
            } else if (choice == "2") {
                std::string name;
                std::cout << "Instructor: ";
                std::getline(std::cin, name);
                displayResults(sendRequest("QUERY INSTRUCTOR " + name));
            } else if (choice == "3") {
                displayResults(sendRequest("QUERY ALL"));
            } else if (choice == "4") {
                std::cout << sendRequest("LOGOUT");
                loggedIn = false;
            } else {
                std::cout << "Invalid choice" << std::endl;
            }
        }
    }
    
    disconnect();
    std::cout << "Goodbye!" << std::endl;
    return 0;
}
