#ifndef SERVER_PROTOCOL_H
#define SERVER_PROTOCOL_H

#include "../database/course_db.h"

#include <string>
#include <vector>

enum class CommandType {
    Empty,
    Ping,
    Help,
    Quit,
    ListAll,
    QueryCode,
    QueryInstructor,
    QuerySemester,
    Login,
    Register,
    Add,
    Update,
    DeleteCourse,
    Encrypt,
    Status,
    Connections,
    Logout,
    Exit,
    DemoConcurrency,
    CloseDemo,
    QueryOnConnections,
    StressTest,
    SequentialTest,
    Invalid
};

struct Request {
    CommandType type;
    std::string argument;
    std::vector<std::string> fields;

    Request() : type(CommandType::Invalid) {}
};

class Protocol {
public:
    static Request parseRequest(const std::string& line);
    static std::string formatCourses(const std::vector<Course>& courses);
    static std::string formatCourse(const Course& course);
    static std::string helpText();

    // Encryption methods
    static std::string encryptXor(const std::string& data, const std::string& key);
    static std::string decryptXor(const std::string& data, const std::string& key);

private:
    static std::string trim(const std::string& value);
    static std::string toUpper(const std::string& value);
    static std::vector<std::string> splitPipe(const std::string& value);
};

#endif  // SERVER_PROTOCOL_H
