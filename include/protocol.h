// Course Timetable System - Shared Client/Server Protocol
// All messages are plain text, terminated by newline.
// Server uses \r\n line endings; clients strip \r on receive.

#ifndef PROTOCOL_H
#define PROTOCOL_H

// ==========================================
// 1. Response prefixes (server -> client)
// ==========================================
constexpr char RESULT_START[] = "RESULT";        // Query result marker: "RESULT count=N\r\n"
constexpr char RESULT_END[]   = "\nEND";         // Query result terminator
constexpr char OK_PREFIX[]    = "OK ";           // Generic success
constexpr char SUCCESS_PREFIX[] = "SUCCESS ";    // Login success
constexpr char FAILURE_PREFIX[] = "FAILURE ";    // Login failure
constexpr char ERROR_PREFIX[]   = "ERROR ";       // Generic error
constexpr char MSG_TERMINATOR[] = "\n";           // Message terminator

// ==========================================
// 2. Command strings (client -> server)
// ==========================================
#define CMD_LIST_ALL         "LIST_ALL"
#define CMD_QUERY_CODE       "QUERY_CODE"
#define CMD_QUERY_INSTRUCTOR "QUERY_INSTRUCTOR"
#define CMD_QUERY_SEMESTER   "QUERY_SEMESTER"
#define CMD_LOGIN            "LOGIN"
#define CMD_REGISTER         "REGISTER"
#define CMD_ADD              "ADD"
#define CMD_UPDATE           "UPDATE"
#define CMD_DELETE           "DELETE"
#define CMD_ENCRYPT          "ENCRYPT"

// ==========================================
// 3. Encryption utilities
// ==========================================
#include <string>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdlib>

constexpr char ENCRYPT_KEY[] = "TIMETABLE2024";

inline std::string xorEncryptDecrypt(const std::string& data) {
    std::string result = data;
    for (size_t i = 0; i < result.size(); i++) {
        result[i] ^= ENCRYPT_KEY[i % (sizeof(ENCRYPT_KEY) - 1)];
    }
    return result;
}

inline std::string toHex(const std::string& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : data) {
        ss << std::setw(2) << static_cast<int>(c);
    }
    return ss.str();
}

inline std::string fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0) return "";
    for (char c : hex) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return "";
    }
    std::string result;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte = hex.substr(i, 2);
        char c = static_cast<char>(std::strtol(byte.c_str(), nullptr, 16));
        result += c;
    }
    return result;
}

inline std::string encryptMessage(const std::string& plaintext) {
    return toHex(xorEncryptDecrypt(plaintext));
}

inline std::string decryptMessage(const std::string& ciphertext) {
    std::string raw = fromHex(ciphertext);
    if (raw.empty()) return "";
    return xorEncryptDecrypt(raw);
}

inline bool isEncrypted(const std::string& msg) {
    if (msg.empty() || msg.size() % 2 != 0) return false;
    for (char c : msg) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

#endif // PROTOCOL_H
