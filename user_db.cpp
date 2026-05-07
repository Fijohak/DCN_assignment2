#include "user_db.h"

#include <fstream>

UserDB::UserDB(const std::string& file) : filename(file) {}

bool UserDB::loadFromFile() {
    std::lock_guard<std::mutex> lock(dbMutex);

    std::ifstream inFile(filename.c_str());
    if (!inFile.is_open()) {
        return false;
    }

    std::vector<User> loadedUsers;
    std::string line;

    if (!std::getline(inFile, line)) {
        return false;
    }

    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    if (line != kCsvHeader) {
        return false;
    }

    while (std::getline(inFile, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> fields = parseCsvLine(line);
        if (fields.size() != 4) {
            continue;
        }

        if (fields[0].empty() || fields[1].empty() || fields[2].empty()) {
            continue;
        }

        if (!isValidRole(fields[3])) {
            continue;
        }

        User user;
        user.username = fields[0];
        user.account = fields[1];
        user.password = fields[2];
        user.role = fields[3];
        loadedUsers.push_back(user);
    }

    users.swap(loadedUsers);
    return true;
}

bool UserDB::saveToFile() const {
    std::lock_guard<std::mutex> lock(dbMutex);
    return saveToFileUnlocked();
}

bool UserDB::accountExists(const std::string& account) const {
    std::lock_guard<std::mutex> lock(dbMutex);

    for (std::vector<User>::const_iterator it = users.begin(); it != users.end(); ++it) {
        if (it->account == account) {
            return true;
        }
    }

    return false;
}

bool UserDB::registerUser(const std::string& username,
                          const std::string& account,
                          const std::string& password,
                          const std::string& role) {
    std::lock_guard<std::mutex> lock(dbMutex);

    if (username.empty() || account.empty() || password.empty() || !isValidRole(role)) {
        return false;
    }

    for (std::vector<User>::const_iterator it = users.begin(); it != users.end(); ++it) {
        if (it->account == account) {
            return false;
        }
    }

    User user;
    user.username = username;
    user.account = account;
    user.password = password;
    user.role = role;

    users.push_back(user);
    if (!saveToFileUnlocked()) {
        users.pop_back();
        return false;
    }

    return true;
}

bool UserDB::loginUser(const std::string& account,
                       const std::string& password,
                       User& outUser) const {
    std::lock_guard<std::mutex> lock(dbMutex);

    for (std::vector<User>::const_iterator it = users.begin(); it != users.end(); ++it) {
        if (it->account == account && it->password == password) {
            outUser = *it;
            return true;
        }
    }

    return false;
}

std::string UserDB::getUserRoleByAccount(const std::string& account) const {
    std::lock_guard<std::mutex> lock(dbMutex);

    for (std::vector<User>::const_iterator it = users.begin(); it != users.end(); ++it) {
        if (it->account == account) {
            return it->role;
        }
    }

    return "";
}

bool UserDB::saveToFileUnlocked() const {
    std::ofstream outFile(filename.c_str(), std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) {
        return false;
    }

    outFile << kCsvHeader << '\n';

    for (std::vector<User>::const_iterator it = users.begin(); it != users.end(); ++it) {
        outFile
            << escapeCsvField(it->username) << ','
            << escapeCsvField(it->account) << ','
            << escapeCsvField(it->password) << ','
            << escapeCsvField(it->role) << '\n';

        if (!outFile.good()) {
            return false;
        }
    }

    outFile.flush();
    return outFile.good();
}

std::vector<std::string> UserDB::parseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];

        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }

    if (inQuotes) {
        return std::vector<std::string>();
    }

    fields.push_back(current);
    return fields;
}

std::string UserDB::escapeCsvField(const std::string& value) {
    bool needQuotes = false;
    std::string escaped;
    escaped.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch == '"') {
            escaped.push_back('"');
            escaped.push_back('"');
            needQuotes = true;
        } else {
            if (ch == ',' || ch == '\n' || ch == '\r') {
                needQuotes = true;
            }
            escaped.push_back(ch);
        }
    }

    if (!needQuotes) {
        return escaped;
    }

    return "\"" + escaped + "\"";
}

bool UserDB::isValidRole(const std::string& role) {
    return role == "student" || role == "admin";
}
