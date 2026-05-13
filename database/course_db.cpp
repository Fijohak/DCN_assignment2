#include "course_db.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {

bool isCourseKeyEqual(const Course& course,
                      const std::string& code,
                      const std::string& section) {
    return course.courseCode == code && course.section == section;
}

}  // namespace

std::string courseToString(const Course& course) {
    std::ostringstream oss;
    oss << "[" << course.semester << "] "
        << course.courseCode << " | "
        << course.courseTitle << " | Sec "
        << course.section << " | "
        << course.instructor << " | "
        << course.day << " "
        << course.startTime << "-" << course.endTime << " | "
        << course.classroom;
    return oss.str();
}

CourseDB::CourseDB(const std::string& file) : filename(file) {}

bool CourseDB::loadFromFile() {
    std::lock_guard<std::mutex> lock(dbMutex);

    std::ifstream inFile(filename.c_str());
    if (!inFile.is_open()) {
        return false;
    }

    std::vector<Course> loadedCourses;
    std::string line;

    if (!std::getline(inFile, line)) {
        return false;
    }

    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    if (line != kCsvHeader) {
        return false;
    }

    while (std::getline(inFile, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> fields = parseCsvLine(line);
        if (fields.size() != 9) {
            continue;
        }

        Course course;
        course.semester = fields[0];
        course.courseCode = fields[1];
        course.courseTitle = fields[2];
        course.section = fields[3];
        course.instructor = fields[4];
        course.day = fields[5];
        course.startTime = fields[6];
        course.endTime = fields[7];
        course.classroom = fields[8];

        loadedCourses.push_back(course);
    }

    courses.swap(loadedCourses);
    return true;
}

bool CourseDB::saveToFile() {
    std::lock_guard<std::mutex> lock(dbMutex);
    return saveToFileUnlocked();
}

std::vector<Course> CourseDB::queryByCourseCode(const std::string& code) const {
    std::lock_guard<std::mutex> lock(dbMutex);
    std::vector<Course> result;

    for (std::vector<Course>::const_iterator it = courses.begin(); it != courses.end(); ++it) {
        if (it->courseCode == code) {
            result.push_back(*it);
        }
    }

    return result;
}

std::vector<Course> CourseDB::queryByInstructor(const std::string& instructor) const {
    std::lock_guard<std::mutex> lock(dbMutex);
    std::vector<Course> result;

    for (std::vector<Course>::const_iterator it = courses.begin(); it != courses.end(); ++it) {
        if (it->instructor == instructor) {
            result.push_back(*it);
        }
    }

    return result;
}

std::vector<Course> CourseDB::queryBySemester(const std::string& semester) const {
    std::lock_guard<std::mutex> lock(dbMutex);
    std::vector<Course> result;

    for (std::vector<Course>::const_iterator it = courses.begin(); it != courses.end(); ++it) {
        if (it->semester == semester) {
            result.push_back(*it);
        }
    }

    return result;
}

std::vector<Course> CourseDB::getAllCourses() const {
    std::lock_guard<std::mutex> lock(dbMutex);
    return courses;
}

bool CourseDB::addCourse(const Course& course) {
    return addCourseDetailed(course) == CourseWriteStatus::Success;
}

CourseWriteStatus CourseDB::addCourseDetailed(const Course& course) {
    std::lock_guard<std::mutex> lock(dbMutex);

    for (std::vector<Course>::const_iterator it = courses.begin(); it != courses.end(); ++it) {
        if (isCourseKeyEqual(*it, course.courseCode, course.section)) {
            return CourseWriteStatus::DuplicateKey;
        }
    }

    courses.push_back(course);
    if (!saveToFileUnlocked()) {
        courses.pop_back();
        return CourseWriteStatus::SaveFailed;
    }

    return CourseWriteStatus::Success;
}

bool CourseDB::updateCourseField(const std::string& code,
                                 const std::string& section,
                                 const std::string& field,
                                 const std::string& newValue) {
    return updateCourseFieldDetailed(code, section, field, newValue) == CourseWriteStatus::Success;
}

CourseWriteStatus CourseDB::updateCourseFieldDetailed(const std::string& code,
                                                      const std::string& section,
                                                      const std::string& field,
                                                      const std::string& newValue) {
    std::lock_guard<std::mutex> lock(dbMutex);

    const std::string fieldName = toUpper(field);

    for (std::vector<Course>::iterator it = courses.begin(); it != courses.end(); ++it) {
        if (!isCourseKeyEqual(*it, code, section)) {
            continue;
        }

        if (fieldName == "TIME") {
            const std::size_t commaPos = newValue.find(',');
            if (commaPos == std::string::npos) {
                return CourseWriteStatus::InvalidValue;
            }

            const std::string newDay = newValue.substr(0, commaPos);
            const std::string timeRange = newValue.substr(commaPos + 1);
            const std::size_t dashPos = timeRange.find('-');
            if (dashPos == std::string::npos) {
                return CourseWriteStatus::InvalidValue;
            }

            const std::string newStartTime = timeRange.substr(0, dashPos);
            const std::string newEndTime = timeRange.substr(dashPos + 1);
            if (newDay.empty() || newStartTime.empty() || newEndTime.empty()) {
                return CourseWriteStatus::InvalidValue;
            }

            const std::string oldDay = it->day;
            const std::string oldStartTime = it->startTime;
            const std::string oldEndTime = it->endTime;

            it->day = newDay;
            it->startTime = newStartTime;
            it->endTime = newEndTime;

            if (!saveToFileUnlocked()) {
                it->day = oldDay;
                it->startTime = oldStartTime;
                it->endTime = oldEndTime;
                return CourseWriteStatus::SaveFailed;
            }

            return CourseWriteStatus::Success;
        }

        std::string* targetField = NULL;
        if (fieldName == "CLASSROOM") {
            targetField = &it->classroom;
        } else if (fieldName == "DAY") {
            targetField = &it->day;
        } else if (fieldName == "START_TIME") {
            targetField = &it->startTime;
        } else if (fieldName == "END_TIME") {
            targetField = &it->endTime;
        } else if (fieldName == "INSTRUCTOR") {
            targetField = &it->instructor;
        } else if (fieldName == "COURSE_TITLE") {
            targetField = &it->courseTitle;
        } else if (fieldName == "SEMESTER") {
            targetField = &it->semester;
        } else {
            return CourseWriteStatus::InvalidField;
        }

        const std::string oldValue = *targetField;
        *targetField = newValue;

        if (!saveToFileUnlocked()) {
            *targetField = oldValue;
            return CourseWriteStatus::SaveFailed;
        }

        return CourseWriteStatus::Success;
    }

    return CourseWriteStatus::NotFound;
}

bool CourseDB::deleteCourse(const std::string& code,
                            const std::string& section) {
    return deleteCourseDetailed(code, section) == CourseWriteStatus::Success;
}

CourseWriteStatus CourseDB::deleteCourseDetailed(const std::string& code,
                                                 const std::string& section) {
    std::lock_guard<std::mutex> lock(dbMutex);

    for (std::vector<Course>::iterator it = courses.begin(); it != courses.end(); ++it) {
        if (!isCourseKeyEqual(*it, code, section)) {
            continue;
        }

        const Course backup = *it;
        const std::vector<Course>::difference_type index = it - courses.begin();
        courses.erase(it);

        if (!saveToFileUnlocked()) {
            courses.insert(courses.begin() + index, backup);
            return CourseWriteStatus::SaveFailed;
        }

        return CourseWriteStatus::Success;
    }

    return CourseWriteStatus::NotFound;
}

bool CourseDB::courseExists(const std::string& code,
                            const std::string& section) const {
    std::lock_guard<std::mutex> lock(dbMutex);

    for (std::vector<Course>::const_iterator it = courses.begin(); it != courses.end(); ++it) {
        if (isCourseKeyEqual(*it, code, section)) {
            return true;
        }
    }

    return false;
}

bool CourseDB::saveToFileUnlocked() const {
    std::ofstream outFile(filename.c_str(), std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) {
        return false;
    }

    outFile << kCsvHeader << '\n';
    if (!outFile.good()) {
        return false;
    }

    for (std::vector<Course>::const_iterator it = courses.begin(); it != courses.end(); ++it) {
        outFile
            << escapeCsvField(it->semester) << ','
            << escapeCsvField(it->courseCode) << ','
            << escapeCsvField(it->courseTitle) << ','
            << escapeCsvField(it->section) << ','
            << escapeCsvField(it->instructor) << ','
            << escapeCsvField(it->day) << ','
            << escapeCsvField(it->startTime) << ','
            << escapeCsvField(it->endTime) << ','
            << escapeCsvField(it->classroom) << '\n';

        if (!outFile.good()) {
            return false;
        }
    }

    outFile.flush();
    if (!outFile.good()) {
        return false;
    }

    outFile.close();
    return !outFile.fail();
}

std::vector<std::string> CourseDB::parseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];

        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }

    if (inQuotes) {
        return std::vector<std::string>();
    }

    fields.push_back(current);
    return fields;
}

std::string CourseDB::escapeCsvField(const std::string& value) {
    bool needQuotes = false;
    std::string escaped;
    escaped.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch == '"') {
            escaped.push_back('"');
            escaped.push_back('"');
            needQuotes = true;
        } else {
            if (ch == ',' || ch == '\n' || ch == '\r') {
                needQuotes = true;
            }
            escaped.push_back(ch);
        }
    }

    if (!needQuotes) {
        return escaped;
    }

    return "\"" + escaped + "\"";
}

std::string CourseDB::toUpper(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return result;
}
