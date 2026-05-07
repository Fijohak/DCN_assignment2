#ifndef USER_DB_H
#define USER_DB_H

#include <mutex>
#include <string>
#include <vector>

struct User {
    std::string username;
    std::string account;
    std::string password;
    std::string role;
};

class UserDB {
public:
    explicit UserDB(const std::string& file);

    bool loadFromFile();
    bool saveToFile() const;

    bool accountExists(const std::string& account) const;

    bool registerUser(const std::string& username,
                      const std::string& account,
                      const std::string& password,
                      const std::string& role);

    bool loginUser(const std::string& account,
                   const std::string& password,
                   User& outUser) const;

    std::string getUserRoleByAccount(const std::string& account) const;

private:
    static constexpr const char* kCsvHeader = "username,account,password,role";

    std::vector<User> users;
    std::string filename;
    mutable std::mutex dbMutex;

    bool saveToFileUnlocked() const;
    static std::vector<std::string> parseCsvLine(const std::string& line);
    static std::string escapeCsvField(const std::string& value);
    static bool isValidRole(const std::string& role);
};

#endif  // USER_DB_H
