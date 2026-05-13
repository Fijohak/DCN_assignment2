// Auth - File-based user authentication.
// Users are stored in a CSV file (Username,Password,Role).
// Default accounts are created if the file does not exist.

#include "auth.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

namespace {

std::string roleToString(UserRole role) {
    switch (role) {
        case UserRole::Admin:   return "ADMIN";
        case UserRole::Student: return "STUDENT";
        default:                return "STUDENT";
    }
}

UserRole roleFromString(const std::string& s) {
    std::string upper = s;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (upper == "ADMIN") return UserRole::Admin;
    return UserRole::Student;
}

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    for (char ch : line) {
        if (ch == ',') {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    fields.push_back(current);
    return fields;
}

}  // namespace

Auth::Auth(const std::string& userFile) : m_filename(userFile) {
    loadFromFile();
}

void Auth::loadFromFile() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream inFile(m_filename.c_str());
    if (!inFile.is_open()) {
        // File does not exist — create default accounts in memory
        m_users["admin"]   = {"admin123",   UserRole::Admin};
        m_users["student"] = {"student123", UserRole::Student};
        saveToFile();  // write the defaults out
        return;
    }

    m_users.clear();
    std::string line;

    // Skip header line
    if (!std::getline(inFile, line)) {
        m_users["admin"]   = {"admin123",   UserRole::Admin};
        m_users["student"] = {"student123", UserRole::Student};
        saveToFile();
        return;
    }

    while (std::getline(inFile, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> fields = splitCsvLine(line);
        if (fields.size() < 3) continue;

        m_users[fields[0]] = {fields[1], roleFromString(fields[2])};
    }

    // Ensure default accounts always exist
    if (m_users.find("admin") == m_users.end())
        m_users["admin"] = {"admin123", UserRole::Admin};
    if (m_users.find("student") == m_users.end())
        m_users["student"] = {"student123", UserRole::Student};
}

bool Auth::saveToFile() const {
    std::ofstream outFile(m_filename.c_str(), std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) return false;

    outFile << "Username,Password,Role\n";
    for (const auto& pair : m_users) {
        outFile << pair.first << ","
                << pair.second.password << ","
                << roleToString(pair.second.role) << "\n";
    }
    return outFile.good();
}

Auth::LoginResult Auth::login(const std::string& username,
                               const std::string& password) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_users.find(username);
    if (it == m_users.end())      return {false, UserRole::None};
    if (it->second.password != password) return {false, UserRole::None};
    return {true, it->second.role};
}

UserRole Auth::getUserRole(const std::string& username) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_users.find(username);
    if (it == m_users.end()) return UserRole::None;
    return it->second.role;
}

Auth::LoginResult Auth::registerUser(const std::string& username,
                                      const std::string& password) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Username must be at least 3 chars, password at least 4
    if (username.length() < 3)  return {false, UserRole::None};
    if (password.length() < 4)  return {false, UserRole::None};

    // Check for duplicate
    if (m_users.find(username) != m_users.end())
        return {false, UserRole::None};

    // All new registrations are Students
    m_users[username] = {password, UserRole::Student};

    if (!saveToFile()) {
        m_users.erase(username);
        return {false, UserRole::None};
    }

    return {true, UserRole::Student};
}
