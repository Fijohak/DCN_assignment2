#include "protocol.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

Request Protocol::parseRequest(const std::string& line) {
    Request request;
    const std::string cleaned = trim(line);

    if (cleaned.empty()) {
        request.type = CommandType::Empty;
        return request;
    }

    const std::size_t spacePos = cleaned.find(' ');
    const std::string command = toUpper(spacePos == std::string::npos
                                            ? cleaned
                                            : cleaned.substr(0, spacePos));
    const std::string rest = spacePos == std::string::npos
                                 ? ""
                                 : trim(cleaned.substr(spacePos + 1));

    if (command == "PING") {
        request.type = CommandType::Ping;
    } else if (command == "HELP") {
        request.type = CommandType::Help;
    } else if (command == "QUIT") {
        request.type = CommandType::Quit;
    } else if (command == "LIST_ALL") {
        request.type = CommandType::ListAll;
    } else if (command == "QUERY_CODE") {
        request.type = rest.empty() ? CommandType::Invalid : CommandType::QueryCode;
        request.argument = rest;
    } else if (command == "QUERY_INSTRUCTOR") {
        request.type = rest.empty() ? CommandType::Invalid : CommandType::QueryInstructor;
        request.argument = rest;
    } else if (command == "QUERY_SEMESTER") {
        request.type = rest.empty() ? CommandType::Invalid : CommandType::QuerySemester;
        request.argument = rest;
    } else if (command == "LOGIN") {
        request.type = CommandType::Login;
        std::istringstream iss(rest);
        std::string username;
        std::string password;
        if (iss >> username >> password) {
            request.fields.push_back(username);
            request.fields.push_back(password);
        } else {
            request.type = CommandType::Invalid;
        }
    } else if (command == "ADD") {
        request.fields = splitPipe(rest);
        request.type = request.fields.size() == 9 ? CommandType::Add : CommandType::Invalid;
    } else if (command == "UPDATE") {
        request.fields = splitPipe(rest);
        request.type = request.fields.size() == 4 ? CommandType::Update : CommandType::Invalid;
    } else if (command == "DELETE") {
        request.fields = splitPipe(rest);
        request.type = request.fields.size() == 2 ? CommandType::DeleteCourse : CommandType::Invalid;
    } else if (command == "ENCRYPT") {
        request.fields = splitPipe(rest);
        request.type = request.fields.size() == 2 ? CommandType::Encrypt : CommandType::Invalid;
        request.argument = rest;
    } else {
        request.type = CommandType::Invalid;
    }

    return request;
}

std::string Protocol::formatCourses(const std::vector<Course>& courses) {
    std::ostringstream oss;
    oss << "RESULT count=" << courses.size() << "\r\n";

    for (std::vector<Course>::const_iterator it = courses.begin(); it != courses.end(); ++it) {
        oss << formatCourse(*it) << "\r\n";
    }

    oss << "END\r\n";
    return oss.str();
}

std::string Protocol::formatCourse(const Course& course) {
    std::ostringstream oss;
    oss << course.courseCode << '|'
        << course.courseTitle << '|'
        << course.section << '|'
        << course.instructor << '|'
        << course.semester << '|'
        << course.day << '|'
        << course.startTime << '|'
        << course.endTime << '|'
        << course.classroom;
    return oss.str();
}

std::string Protocol::helpText() {
    return "OK Commands: PING, HELP, LIST_ALL, QUERY_CODE <course_code>, "
           "QUERY_INSTRUCTOR <instructor>, QUERY_SEMESTER <semester>, "
           "LOGIN <username> <password>, ADD <9 pipe fields>, "
           "UPDATE <code>|<section>|<field>|<new_value>, DELETE <code>|<section>, "
           "ENCRYPT <data>|<key>, QUIT\r\n";
}

std::string Protocol::trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

std::string Protocol::toUpper(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return result;
}

std::vector<std::string> Protocol::splitPipe(const std::string& value) {
    std::vector<std::string> fields;
    std::string current;

    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '|') {
            fields.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(value[i]);
        }
    }

    fields.push_back(trim(current));
    return fields;
}

std::string Protocol::encryptXor(const std::string& data, const std::string& key) {
    if (key.empty()) {
        return data;
    }

    std::string result;
    result.reserve(data.size());

    for (std::size_t i = 0; i < data.size(); ++i) {
        result.push_back(static_cast<char>(data[i] ^ key[i % key.size()]));
    }

    return result;
}

std::string Protocol::decryptXor(const std::string& data, const std::string& key) {
    // XOR decryption is the same operation as encryption
    return encryptXor(data, key);
}
