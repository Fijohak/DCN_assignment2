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

// Protocol constants
#define PROTOCOL_VERSION "1.0"
#define MSG_TERMINATOR "\n"
#define RESULT_START "RESULT\n"
#define RESULT_END "\nEND\n"
#define SUCCESS_PREFIX "SUCCESS "
#define FAILURE_PREFIX "FAILURE "
#define ERROR_PREFIX "ERROR "

// Command strings
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

/*
 * ============================================================
 *  Secure Communication Module (Bonus Feature)
 * ============================================================
 *  Encryption: XOR cipher with rotating key
 *  Key: "TIMETABLE2024" (12-byte rotating key)
 *
 *  How it works:
 *  1. Each byte of plaintext is XORed with a key byte
 *  2. Key rotates: key[i % key_length]
 *  3. XOR is symmetric: encrypt == decrypt
 *  4. Encrypted data is encoded as hex string for safe TCP transport
 *
 *  Security Level: Basic (suitable for student project demo)
 *  - XOR with rotating key is stronger than single-byte XOR
 *  - Not cryptographically secure, but demonstrates encryption concept
 *  - Prevents plaintext eavesdropping on the network
 *
 *  Protocol Extension:
 *  After connection, client sends: ENCRYPT <key_index>
 *  Server responds: ENCRYPT_OK
 *  All subsequent messages are encrypted
 *
 *  Example:
 *  Client -> Server: ENCRYPT 1
 *  Server -> Client: ENCRYPT_OK
 *  Client -> Server: <encrypted_hex>  (was: LOGIN admin admin123)
 *  Server -> Client: <encrypted_hex>  (was: SUCCESS Login successful)
 * ============================================================
 */

#include <string>
#include <sstream>
#include <iomanip>

// Encryption key (12 bytes, rotating)
const std::string ENCRYPT_KEY = "TIMETABLE2024";

// XOR encrypt/decrypt (symmetric)
inline std::string xorEncryptDecrypt(const std::string& data) {
    std::string result = data;
    for (size_t i = 0; i < result.size(); i++) {
        result[i] ^= ENCRYPT_KEY[i % ENCRYPT_KEY.size()];
    }
    return result;
}

// Convert binary string to hex string for safe TCP transport
inline std::string toHex(const std::string& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : data) {
        ss << std::setw(2) << (int)c;
    }
    return ss.str();
}

// Convert hex string back to binary string
inline std::string fromHex(const std::string& hex) {
    std::string result;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte = hex.substr(i, 2);
        char c = (char)strtol(byte.c_str(), NULL, 16);
        result += c;
    }
    return result;
}

// Encrypt a plaintext message: XOR -> Hex
inline std::string encryptMessage(const std::string& plaintext) {
    std::string xored = xorEncryptDecrypt(plaintext);
    return toHex(xored);
}

// Decrypt a received message: Hex -> XOR
inline std::string decryptMessage(const std::string& ciphertext) {
    std::string raw = fromHex(ciphertext);
    return xorEncryptDecrypt(raw);
}

// Check if a string looks like encrypted hex data
inline bool isEncrypted(const std::string& data) {
    if (data.length() < 4) return false;
    for (char c : data) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

#endif // PROTOCOL_H
