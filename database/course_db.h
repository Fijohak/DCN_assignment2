#ifndef COURSE_DB_H
#define COURSE_DB_H

#include <mutex>
#include <string>
#include <vector>

struct Course {
    std::string semester;
    std::string courseCode;
    std::string courseTitle;
    std::string section;
    std::string instructor;
    std::string day;
    std::string startTime;
    std::string endTime;
    std::string classroom;
};

enum class CourseWriteStatus {
    Success,
    DuplicateKey,
    NotFound,
    InvalidField,
    InvalidValue,
    InvalidTime,
    InstructorConflict,
    ClassroomConflict,
    SaveFailed
};

std::string courseToString(const Course& course);

class CourseDB {
public:
    explicit CourseDB(const std::string& file);

    bool loadFromFile();
    bool saveToFile();

    std::vector<Course> queryByCourseCode(const std::string& code) const;
    std::vector<Course> queryByInstructor(const std::string& instructor) const;
    std::vector<Course> queryBySemester(const std::string& semester) const;
    std::vector<Course> getAllCourses() const;

    bool addCourse(const Course& course);
    CourseWriteStatus addCourseDetailed(const Course& course);
    bool updateCourseField(const std::string& code,
                           const std::string& section,
                           const std::string& field,
                           const std::string& newValue);
    CourseWriteStatus updateCourseFieldDetailed(const std::string& code,
                                                const std::string& section,
                                                const std::string& field,
                                                const std::string& newValue);
    bool deleteCourse(const std::string& code,
                      const std::string& section);
    CourseWriteStatus deleteCourseDetailed(const std::string& code,
                                           const std::string& section);

    bool courseExists(const std::string& code,
                      const std::string& section) const;

private:
    static constexpr const char* kCsvHeader =
        "semester,course_code,course_title,section,instructor,day,start_time,end_time,classroom";

    std::vector<Course> courses;
    std::string filename;
    mutable std::mutex dbMutex;

    bool saveToFileUnlocked() const;
    CourseWriteStatus validateScheduleUnlocked(const Course& candidate,
                                               const Course* self) const;
    static std::vector<std::string> parseCsvLine(const std::string& line);
    static std::string escapeCsvField(const std::string& value);
    static std::string toUpper(const std::string& text);
    static bool parseTimeMinutes(const std::string& text, int& minutes);
};

#endif  // COURSE_DB_H
