#ifndef SERVER_AUTH_H
#define SERVER_AUTH_H

#include <string>

class Auth {
public:
    bool login(const std::string& username, const std::string& password) const;
};

#endif  // SERVER_AUTH_H
