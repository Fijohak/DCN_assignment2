#include "auth.h"

bool Auth::login(const std::string& username, const std::string& password) const {
    return username == "admin" && password == "password123";
}
