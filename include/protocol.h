#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
 * ============================================================
 *  Course Timetable Inquiry System - Communication Protocol
 * ============================================================
 *  Version: 1.0
 *  Transport: TCP (Winsock)
 *  Encoding: ASCII text, newline-terminated
 *
 *  --- Protocol Overview ---
 *
 *  All messages are plain text, terminated by newline '\n'.
 *  The server and client communicate using request/response pattern.
 *  Each request from client triggers exactly one response from server.
 *
 *  --- 1. Connection ---
 *  Client connects to server via TCP.
 *  Server sends welcome message immediately:
 *    Welcome to Course Timetable System
 *
 *  --- 2. Authentication ---
 *
 *  2.1 LOGIN
 *    Request:  LOGIN <username> <password>
 *    Success:  SUCCESS Login successful. Role: <Admin|Student>
 *    Failure:  FAILURE Invalid username or password
 *
 *  2.2 REGISTER
 *    Request:  REGISTER <username> <password>
 *    Success:  SUCCESS Registration successful. You can now login.
 *    Failure:  FAILURE Username already exists
 *              FAILURE Username must be at least 3 characters
 *              FAILURE Password must be at least 4 characters
 *
 *  2.3 LOGOUT
 *    Request:  LOGOUT
 *    Success:  SUCCESS Logged out
 *    Failure:  FAILURE Not logged in
 *
 *  --- 3. Query Operations (Student & Admin) ---
 *
 *  3.1 Search by Course Code
 *    Request:  QUERY CODE <course_code>
 *    Response: RESULT
 *              <course_code> | <title> | Sec <section> | <instructor> | <time> | <classroom>
 *              ...
 *              END
 *    (If no results: RESULT\nNo courses found with code: <code>\nEND)
 *
 *  3.2 Search by Instructor
 *    Request:  QUERY INSTRUCTOR <name>
 *    Response: RESULT\n<results>\nEND
 *
 *  3.3 View All Courses
 *    Request:  QUERY ALL
 *    Response: RESULT\n<all_courses>\nEND
 *
 *  3.4 Search by Time (Advanced)
 *    Request:  QUERY TIME <keyword>
 *    Response: RESULT\n<results>\nEND
 *
 *  3.5 Search by Title (Advanced)
 *    Request:  QUERY TITLE <keyword>
 *    Response: RESULT\n<results>\nEND
 *
 *  3.6 Search by Classroom (Advanced)
 *    Request:  QUERY CLASSROOM <keyword>
 *    Response: RESULT\n<results>\nEND
 *
 *  3.7 Advanced Search
 *    Request:  QUERY ADVANCED <field> <operator> <value>
 *    Fields:   code, title, section, instructor, time, classroom
 *    Operators:
 *      =  or eq        - Exact match
 *      ~= or contains  - Substring match
 *      != or ne        - Not equal
 *      ^= or startswith - Starts with
 *    Response: RESULT\n<results>\nEND
 *
 *  --- 4. Admin Operations (Admin only) ---
 *
 *  4.1 Add Course
 *    Request:  ADD <code> <title> <section> <instructor> <time> <classroom>
 *    Success:  SUCCESS Course added
 *    Failure:  FAILURE Admin privileges required
 *              FAILURE Usage: ADD <code> <title> <section> <instructor> <time> <classroom>
 *
 *  4.2 Update Course
 *    Request:  UPDATE <code> <section> <field> <new_value>
 *    Fields:   title, instructor, time, classroom
 *    Success:  SUCCESS Course updated
 *    Failure:  FAILURE Course not found
 *              FAILURE Unknown field: <field>
 *
 *  4.3 Delete Course
 *    Request:  DELETE <code> <section>
 *    Success:  SUCCESS Course deleted
 *    Failure:  FAILURE Course not found
 *
 *  --- 5. Error Responses ---
 *    ERROR Unknown command
 *    FAILURE Please login first
 *    FAILURE Admin privileges required
 *
 *  --- 6. Response Format Summary ---
 *    Success:  SUCCESS <message>
 *    Failure:  FAILURE <error_message>
 *    Query:    RESULT\n<data>\nEND
 *    Error:    ERROR <message>
 *
 * ============================================================
 */

// ==========================================
// 1. Protocol Constants
// ==========================================
constexpr char PROTOCOL_VERSION[] = "1.0";
constexpr char MSG_TERMINATOR[] = "\n";          // 消息结束符 (单换行)
constexpr char RESULT_START[] = "RESULT\n";      // 查询结果起始标识
constexpr char RESULT_END[] = "\nEND";           
constexpr char SUCCESS_PREFIX[] = "SUCCESS ";    // 注意尾部空格
constexpr char FAILURE_PREFIX[] = "FAILURE ";    // 注意尾部空格
constexpr char ERROR_PREFIX[] = "ERROR ";       // 注意尾部空格

// ==========================================
// 2. Command Strings
// ==========================================
#define CMD_LOGIN    "LOGIN"
#define CMD_REGISTER "REGISTER"
#define CMD_LOGOUT   "LOGOUT"
#define CMD_QUERY    "QUERY"
#define CMD_ADD      "ADD"
#define CMD_UPDATE   "UPDATE"
#define CMD_DELETE   "DELETE"

// Query types
#define QUERY_CODE       "CODE"
#define QUERY_INSTRUCTOR "INSTRUCTOR"
#define QUERY_ALL        "ALL"
#define QUERY_TIME       "TIME"
#define QUERY_TITLE      "TITLE"
#define QUERY_CLASSROOM  "CLASSROOM"
#define QUERY_ADVANCED   "ADVANCED"

// ==========================================
// 3. Secure Communication Module
// ==========================================
#include <string>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdlib>

// Encryption key (12 bytes, rotating)
constexpr char ENCRYPT_KEY[] = "TIMETABLE2024";

// XOR encrypt/decrypt (symmetric)
inline std::string xorEncryptDecrypt(const std::string& data) {
    std::string result = data;
    for (size_t i = 0; i < result.size(); i++) {
        result[i] ^= ENCRYPT_KEY[i % (sizeof(ENCRYPT_KEY) - 1)]; // 安全处理数组边界
    }
    return result;
}

// Convert binary string to hex string for safe TCP transport
inline std::string toHex(const std::string& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : data) {
        ss << std::setw(2) << static_cast<int>(c);
    }
    return ss.str();
}

// Convert hex string back to binary string (严格校验)
inline std::string fromHex(const std::string& hex) {
    // 1. 检查长度是否为偶数
    if (hex.size() % 2 != 0) {
        return ""; // 无效hex
    }
    // 2. 检查所有字符是否为十六进制
    for (char c : hex) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return ""; // 非法字符
        }
    }
    // 3. 安全转换
    std::string result;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte = hex.substr(i, 2);
        char c = static_cast<char>(std::strtol(byte.c_str(), nullptr, 16));
        result += c;
    }
    return result;
}

// Encrypt a plaintext message: XOR -> Hex
inline std::string encryptMessage(const std::string& plaintext) {
    std::string xored = xorEncryptDecrypt(plaintext);
    return toHex(xored);
}

// Decrypt a received message: Hex -> XOR (严格校验)
inline std::string decryptMessage(const std::string& ciphertext) {
    std::string raw = fromHex(ciphertext);
    if (raw.empty()) {
        return ""; // 解密失败
    }
    return xorEncryptDecrypt(raw);
}

// Check if a message is encrypted (hex-encoded ciphertext)
inline bool isEncrypted(const std::string& msg) {
    if (msg.empty() || msg.size() % 2 != 0) return false;
    for (char c : msg) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

// ==========================================
// 4. Critical Fix: Safe Command Builder 
// ==========================================
#include <vector>

/**
 * @brief 安全生成查询命令 (避免手动拼接空格错误)
 * @param type 查询类型 (如 QUERY_CODE)
 * @param args 参数列表 (如 {"CS101"})
 * @return 完整命令字符串 (例: "QUERY CODE CS101")
 */
inline std::string makeQueryCommand(const std::string& type, const std::vector<std::string>& args = {}) {
    std::ostringstream oss;
    oss << "QUERY " << type; //确保 QUERY 和类型间有空格
    for (const auto& arg : args) {
        oss << " " << arg; //
    }
    return oss.str();
}

/**
 * @brief 安全生成管理命令 (ADD/UPDATE/DELETE)
 * @param command 基础命令 (如 CMD_ADD)
 * @param args 参数列表
 * @return 完整命令字符串 (例: "ADD CS101 Math 01 ProfX ...")
 */
inline std::string makeAdminCommand(const std::string& command, const std::vector<std::string>& args) {
    std::ostringstream oss;
    oss << command;
    for (const auto& arg : args) {
        oss << " " << arg;
    }
    return oss.str();
}

#endif // PROTOCOL_H