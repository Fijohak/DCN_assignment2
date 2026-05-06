#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>

// 课程记录结构
struct CourseRecord {
    std::string courseCode;
    std::string courseTitle;
    std::string section;
    std::string instructor;
    std::string time;
    std::string classroom;
};

// 用户角色
enum UserRole { ROLE_STUDENT, ROLE_ADMIN, ROLE_NONE };

#endif
