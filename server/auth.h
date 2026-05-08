#ifndef SERVER_AUTH_H
#define SERVER_AUTH_H

#include <map>
#include <mutex>
#include <string>

enum class UserRole { Student, Admin, None };

class Auth {
public:
    explicit Auth(const std::string& userFile);

    struct LoginResult {
        bool success;
        UserRole role;
    };

    LoginResult login(const std::string& username, const std::string& password) const;
    LoginResult registerUser(const std::string& username, const std::string& password);

private:
    struct UserEntry {
        std::string password;
        UserRole role;
    };

    std::string m_filename;
    std::map<std::string, UserEntry> m_users;
    mutable std::mutex m_mutex;

    void loadFromFile();
    bool saveToFile() const;
};

#endif  // SERVER_AUTH_H
