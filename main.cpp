#include "course_db.h"
#include "user_db.h"

#include <iostream>
#include <vector>

int main() {
    CourseDB db("courses.csv");

    if (!db.loadFromFile()) {
        std::cout << "courses.csv not found, starting with empty database." << std::endl;
    }

    std::vector<Course> spring = db.queryBySemester("2026 Spring");
    std::cout << "2026 Spring count = " << spring.size() << std::endl;
    for (std::size_t i = 0; i < spring.size(); ++i) {
        std::cout << "  " << courseToString(spring[i]) << std::endl;
    }

    std::vector<Course> comp3003 = db.queryByCourseCode("COMP3003");
    std::cout << "Query result for COMP3003:" << std::endl;
    for (std::size_t i = 0; i < comp3003.size(); ++i) {
        std::cout << "  " << courseToString(comp3003[i]) << std::endl;
    }

    Course newCourse;
    newCourse.semester = "2026 Spring";
    newCourse.courseCode = "COMP4004";
    newCourse.courseTitle = "Database Systems";
    newCourse.section = "02";
    newCourse.instructor = "Dr. Chen";
    newCourse.day = "Thu";
    newCourse.startTime = "14:00";
    newCourse.endTime = "15:50";
    newCourse.classroom = "B301";

    if (db.addCourse(newCourse)) {
        std::cout << "Added course: " << courseToString(newCourse) << std::endl;
    } else {
        std::cout << "Add course failed (maybe duplicate or file save failed)." << std::endl;
    }

    if (db.updateCourseField("COMP3003", "01", "TIME", "Mon,10:00-12:00")) {
        std::cout << "Updated TIME for COMP3003 section 01." << std::endl;
    } else {
        std::cout << "TIME update failed." << std::endl;
    }

    if (db.updateCourseField("COMP4004", "02", "CLASSROOM", "B305")) {
        std::cout << "Updated classroom for COMP4004 section 02." << std::endl;
    } else {
        std::cout << "Update failed." << std::endl;
    }

    std::vector<Course> byInstructor = db.queryByInstructor("Dr. Chen");
    std::cout << "Courses taught by Dr. Chen:" << std::endl;
    for (std::size_t i = 0; i < byInstructor.size(); ++i) {
        std::cout << "  " << courseToString(byInstructor[i]) << std::endl;
    }

    if (db.deleteCourse("COMP4004", "02")) {
        std::cout << "Deleted COMP4004 section 02." << std::endl;
    } else {
        std::cout << "Delete failed." << std::endl;
    }

    std::vector<Course> spring2026 = db.queryBySemester("2026 Spring");
    std::cout << "2026 Spring count after updates = " << spring2026.size() << std::endl;

    UserDB userDb("users.csv");

    if (!userDb.loadFromFile()) {
        std::cout << "Failed to load users.csv." << std::endl;
    } else {
        std::cout << "Loaded users.csv successfully." << std::endl;
    }

    std::cout << "Does account root exist? "
              << (userDb.accountExists("root") ? "Yes" : "No") << std::endl;

    if (userDb.registerUser("Alice", "alice001", "123456", "student")) {
        std::cout << "Registered Alice / alice001 successfully." << std::endl;
    } else {
        std::cout << "Register Alice / alice001 failed." << std::endl;
    }

    if (userDb.registerUser("Alice", "alice002", "abc123", "student")) {
        std::cout << "Registered Alice / alice002 successfully." << std::endl;
    } else {
        std::cout << "Register Alice / alice002 failed." << std::endl;
    }

    if (userDb.registerUser("Another Alice", "alice001", "zzz999", "student")) {
        std::cout << "Registered duplicate account alice001 unexpectedly succeeded." << std::endl;
    } else {
        std::cout << "Register duplicate account alice001 failed as expected." << std::endl;
    }

    User loggedInUser;
    if (userDb.loginUser("root", "admin123", loggedInUser)) {
        std::cout << "Admin login success: "
                  << loggedInUser.username << " | "
                  << loggedInUser.account << " | "
                  << loggedInUser.role << std::endl;
    } else {
        std::cout << "Admin login failed." << std::endl;
    }

    if (userDb.loginUser("alice001", "wrongpass", loggedInUser)) {
        std::cout << "alice001 login with wrong password unexpectedly succeeded." << std::endl;
    } else {
        std::cout << "alice001 login with wrong password failed as expected." << std::endl;
    }

    std::cout << "Role of root: " << userDb.getUserRoleByAccount("root") << std::endl;

    return 0;
}
