# Course Timetable Inquiry System
## Data Communications and Networking -- Assignment 2

---

## Project Overview

A **client-server-based course timetable inquiry system** built with **Windows Socket Programming (Winsock) in C++**. The system supports multiple concurrent users, provides reliable access to structured timetable data stored in CSV files, and includes a modern **Web UI** for easy demonstration of all features.

**Key Features:**
- Multi-threaded TCP server handling concurrent client connections
- Structured course database with CSV file storage
- Role-based access control (Student / Administrator)
- Real-time course CRUD operations (Create, Read, Update, Delete)
- Built-in HTTP server for Web UI access
- XOR encryption demo module
- Concurrency stress testing tools

---

## Quick Start

### Option 1: One-Click Launch (Recommended)

Double-click **`start_system.bat`** and choose client type:

```
Choose client type:
  1. CMD Client (console)    -- Command-line interface
  2. GUI Client (graphical)  -- Win32 GUI interface
Enter choice (1 or 2):
```

The script will automatically:
1. Check if executables exist
2. Start the server (minimized window)
3. Wait 2 seconds for server readiness
4. Launch the chosen client
5. Auto-close server when client exits

### Option 2: Manual Launch

**1. Compile (if executables are missing):**

```bash
# Compile server
g++ -std=c++11 -pthread -o server.exe server/main.cpp server/server.cpp server/client_handler.cpp server/protocol.cpp server/auth.cpp server/logger.cpp database/course_db.cpp database/user_db.cpp -lws2_32

# Or use Makefile
make
```

**2. Start the server:**
```bash
server.exe
```

**3. Access Web UI:**
Open browser -> **http://localhost:8080**

---

## Web UI Access

The server includes a built-in HTTP server on **port 8080**. Simply open a browser and navigate to `http://localhost:8080` to access the full-featured Web interface.

**No additional web server or dependencies required!**

---

## Project Structure

```
DCN_assignment2/
│
├── server/                          # Server-side source code
│   ├── main.cpp                     # Server entry point
│   ├── server.cpp / server.h        # Core server (TCP + HTTP)
│   ├── client_handler.cpp / .h      # Client connection handler
│   ├── protocol.cpp / protocol.h    # Communication protocol parser
│   ├── auth.cpp / auth.h            # User authentication
│   └── logger.cpp / logger.h        # Logging system
│
├── database/                        # Database layer
│   ├── course_db.cpp / .h           # Course database (CSV-based)
│   ├── user_db.cpp / .h             # User database (CSV-based)
│   ├── courses.csv                  # Course data file
│   └── users.csv                    # User data file
│
├── gui_web/                         # Web UI
│   └── index.html                   # Single-page Web application
│
├── include/                         # Shared headers
│   ├── common.h                     # Common data structures
│   ├── protocol.h                   # Protocol definitions
│   └── network_client.h             # Network client abstraction
│
├── client/                          # Client applications
│   ├── client.cpp                   # Console-based client
│   ├── gui_client.cpp               # Win32 GUI client
│   └── network_client.cpp           # Network communication layer
│
├── logs/                            # Server logs
│   └── server.log                   # Runtime log file
│
├── Makefile                         # Build automation
├── README.md                        # This document
├── start_system.bat                 # One-click launcher
└── test_client.bat                  # Test batch file
```

---

## Default Accounts

| Role | Username | Password | Permissions |
|------|----------|----------|-------------|
| **Student** | `student` | `student123` | Query only |
| **Administrator** | `admin` | `admin123` | Full access (CRUD) |

> **New user registration**: Users can self-register via the Web UI or CMD client. New accounts default to Student role. User data persists in `database/users.csv` across server restarts.

---

## Functional Requirements -- Implementation Details

### (1) Database Module

**Requirement:**
- Store course timetable data: Course code, Course title, Section, Instructor, Time (day and duration), Classroom
- File-based storage (CSV, TXT) or embedded database (SQLite)

**Implementation:**
- **CSV file storage** (`database/courses.csv`) with 9 fields:
  - `semester`, `course_code`, `course_title`, `section`, `instructor`, `day`, `start_time`, `end_time`, `classroom`
- Data loaded into memory at server startup for fast access
- All modifications immediately persisted to CSV file
- **Thread-safe** with `std::mutex` for concurrent access protection

**Sample data:**
```
semester,course_code,course_title,section,instructor,day,start_time,end_time,classroom
2026 Spring,COMP3003,Operating Systems,01,Dr. Chen,Mon,10:00,12:00,B201
2026 Spring,COMP3003,Operating Systems,02,Dr. Chen,Fri,10:00,12:00,B202
2026 Spring,COMP2001,Data Structures,01,Dr. Li,Tue,09:30,11:00,A101
2026 Fall,COMP2001,Data Structures,02,Dr. Wang,Thu,13:00,14:30,A102
2026 Fall,COMP1001,Introduction to Programming,01,Dr. Zhang,Mon,08:00,10:00,C305
2027 Spring,COMP4002,Computer Networks,01,Dr. Liu,Fri,15:00,17:00,D410
```

**Source code:** `database/course_db.cpp`, `database/course_db.h`

---

### (2) Query Module

**Requirement:**
- Search by course code
- Search by instructor
- View all courses for a given semester
- Display results in clear, readable format

**Implementation:**

| Query Type | Command |
|------------|---------|
| All courses | `LIST_ALL` |
| By course code | `QUERY_CODE <code>`|
| By instructor | `QUERY_INSTRUCTOR <name>`|
| By semester | `QUERY_SEMESTER <semester>`|

**Response format:** Pipe-delimited 9-field records wrapped in `RESULT...END` block

**Web UI:** Results displayed in a sortable table with columns: Code | Title | Sec | Instructor | Semester | Day | Start | End | Room

**Source code:** `server/protocol.cpp` (parsing), `database/course_db.cpp` (queries), `gui_web/index.html` (display)

---

### (3) User Management Module

**Requirement:**
- Support at least two roles: Student (default, query-only) and Administrator (full access)
- Administrator functions: add, update, delete timetable records
- Authentication (username/password) required for administrator access

**Implementation:**
- **Two roles with distinct permissions:**
  - `Student`: Query-only access after login
  - `Admin`: Full CRUD access after login
- **Authentication flow:**
  1. Client sends `LOGIN <username> <password>`
  2. Server validates against `database/users.csv`
  3. Server returns `SUCCESS Login successful. Role: <role>` or `FAILURE Invalid username or password`
- **Session management:** Server maintains login state per connection
- **User registration:** `REGISTER <username> <password>` creates new Student account
- **Data persistence:** User data stored in `database/users.csv`, survives server restarts

**Permission enforcement:**
```cpp
// In client_handler.cpp -- every admin command checks:
if (!loggedIn || !isAdmin) {
    return "ERROR Permission denied\r\n";
}
```

**Web UI behavior:**
- Student login: Admin Panel shows permission warning banner
- Admin login: Full Add/Update/Delete functionality enabled

**Source code:** `server/auth.cpp`, `server/auth.h`, `server/client_handler.cpp`

---

### (4) Information Update Module

**Requirement:**
- Administrators can modify specific fields (e.g., time or classroom)
- Add new course entries
- Delete outdated entries
- Changes must be reflected immediately for all connected clients


**Supported update fields:** `course_title`, `instructor`, `day`, `start_time`, `end_time`, `classroom`, `semester`

**Immediate effect:** All modifications are:
1. Written to CSV file immediately
2. Reflected in subsequent queries from memory
3. Visible to all connected clients on next query

**Source code:** `database/course_db.cpp` (`addCourse()`, `updateCourseField()`, `deleteCourse()`)

---

### (5) Networking and Concurrency Module

**Requirement:**
- Implement using Windows Sockets (Winsock) in C++
- Support at least 5 concurrent client connections
- Use multithreading (e.g., std::thread) or I/O multiplexing (e.g., select())

**Implementation:**

**Architecture:**
```
+---------------------------------------------------+
|                   Server                          |
|  +-------------+  +-------------+                 |
|  | TCP Server  |  | HTTP Server |                 |
|  | (Port 54000)|  | (Port 8080) |                 |
|  +------+------+  +------+------+                 |
|         |                |                        |
|  +------+------+  +------+------+                 |
|  | std::thread |  | std::thread |                 |
|  | per client  |  | per request |                 |
|  +-------------+  +-------------+                 |
|                                                   |
|  +--------------------------------------------+   |
|  |         CourseDB (thread-safe)             |   |
|  |         protected by std::mutex            |   |
|  +--------------------------------------------+   |
+---------------------------------------------------+
```

**Key technical details:**
- **Winsock API:** `WSAStartup`, `socket()`, `bind()`, `listen()`, `accept()`, `send()`, `recv()`
- **Multithreading:** Each client connection spawns a `std::thread`
- **Backlog:** Server `listen()` backlog set to 5 (minimum requirement)
- **Thread safety:** `std::mutex` protects shared CourseDB and UserDB
- **Built-in HTTP server:** Serves Web UI on port 8080

**Concurrency demonstration tools (in Web UI):**
- **Open 5/10 Connections:** Creates persistent TCP connections that stay alive
- **Stress Test:** Opens N connections simultaneously to test concurrent handling
- **Sequential Test:** Opens N connections one-by-one for comparison
- **Real-time monitoring:** Active Connections / Total Handled counters

**Server startup output:**
```
Course Timetable Server running on port 54000
HTTP Server listening on port 8080
Press Ctrl+C to stop the server.
```

**Source code:** `server/server.cpp`

---

### (6) Communication Protocol

**Requirement:**
- Define a simple application-layer protocol
- Include request format, response format, and error messages

**Implementation:**

#### Request Format
```
PING                              # Connection test
HELP                              # Display help
QUIT                              # Disconnect
LOGIN <username> <password>       # Authenticate
LOGOUT                            # Clear login state
EXIT                              # Close connection
REGISTER <username> <password>    # Create new account
LIST_ALL                          # View all courses
QUERY_CODE <code>                 # Search by course code
QUERY_INSTRUCTOR <name>           # Search by instructor
QUERY_SEMESTER <semester>         # Search by semester
ADD <sem>|<code>|<title>|<sec>|<inst>|<day>|<start>|<end>|<room>  # Add course (Admin)
UPDATE <code>|<sec>|<field>|<val> # Update course (Admin)
DELETE <code>|<sec>               # Delete course (Admin)
ENCRYPT <text>|<key>              # XOR encryption demo
STATUS                            # Server status
CONNECTIONS                       # Active connections list
```

#### Response Format
```
SUCCESS <message>                 # Operation succeeded
FAILURE <reason>                  # Operation failed (auth)
ERROR <message>                   # Error occurred
OK <message>                      # Simple success
RESULT count=N                    # Query result header
<data>                            # Pipe-delimited data rows
END                               # Query result footer
```

#### Error Handling Examples
```
> INVALID COMMAND
ERROR Invalid command

> QUERY_CODE
ERROR Please login first

> ADD test course
ERROR Permission denied

> LOGIN wrong pass
FAILURE Invalid username or password
```

**Source code:** `server/protocol.cpp` (parser), `server/client_handler.cpp` (command execution)

---

### (7) Non-Functional Requirements

**Requirement:**
- Handle invalid or malformed requests gracefully
- Prevent crashes from unexpected input
- Ensure reasonable response time under concurrent access
- Maintain clean, modular code structure

**Implementation:**

| Requirement | Implementation |
|-------------|---------------|
| **Error handling** | All commands validate parameter count; returns descriptive error messages |
| **Input validation** | `trim()` removes whitespace; `toUpper()` normalizes commands; buffer overflow protection (4096 byte limit) |
| **Response time** | In-memory queries -- no disk I/O during search operations |
| **Code structure** | Modular design: `server/`, `database/`, `gui_web/`, `client/` separation; clear comments throughout |
| **Crash prevention** | Try-catch around thread operations; socket error handling; graceful degradation |

---



## Protocol Communication Examples

### Student Query Flow
```
-> LOGIN student student123
<- SUCCESS Login successful. Role: Student

-> LIST_ALL
<- RESULT count=7
  2026 Spring|COMP3003|Operating Systems|01|Dr. Chen|Mon|10:00|12:00|B201
  2026 Spring|COMP3003|Operating Systems|02|Dr. Chen|Fri|10:00|12:00|B202
  2026 Spring|COMP2001|Data Structures|01|Dr. Li|Tue|09:30|11:00|A101
  2026 Fall|COMP2001|Data Structures|02|Dr. Wang|Thu|13:00|14:30|A102
  2026 Fall|COMP1001|Introduction to Programming|01|Dr. Zhang|Mon|08:00|10:00|C305
  2027 Spring|COMP4002|Computer Networks|01|Dr. Liu|Fri|15:00|17:00|D410
  2025-2026|COMP3003|Test|1A|Dr.Test|Mon|10:00|12:00|E101
  END

-> QUERY_CODE COMP3003
<- RESULT count=3
  2026 Spring|COMP3003|Operating Systems|01|Dr. Chen|Mon|10:00|12:00|B201
  2026 Spring|COMP3003|Operating Systems|02|Dr. Chen|Fri|10:00|12:00|B202
  2025-2026|COMP3003|Test|1A|Dr.Test|Mon|10:00|12:00|E101
  END

-> QUERY_INSTRUCTOR Dr. Chen
<- RESULT count=2
  2026 Spring|COMP3003|Operating Systems|01|Dr. Chen|Mon|10:00|12:00|B201
  2026 Spring|COMP3003|Operating Systems|02|Dr. Chen|Fri|10:00|12:00|B202
  END

-> LOGOUT
<- OK Logged out successfully
```

### Administrator Update Flow
```
-> LOGIN admin admin123
<- SUCCESS Login successful. Role: Admin

-> UPDATE COMP3003|01|classroom|A101
<- OK Record updated

-> QUERY_CODE COMP3003
<- RESULT count=3
  2026 Spring|COMP3003|Operating Systems|01|Dr. Chen|Mon|10:00|12:00|A101  (Updated!)
  2026 Spring|COMP3003|Operating Systems|02|Dr. Chen|Fri|10:00|12:00|B202
  2025-2026|COMP3003|Test|1A|Dr.Test|Mon|10:00|12:00|E101
  END

-> DELETE COMP3003|1A
<- OK Record deleted
```

### User Registration Flow
```
-> REGISTER newuser pass1234
<- SUCCESS Registration successful. Role: Student
```

### Encryption Demo Flow
```
-> ENCRYPT HelloWorld|key
<- RESULT count=4
  Original: [HelloWorld]
  Key: [key]
  Encrypted (hex): [070a0e1f160b1c0b1d]
  Decrypted: [HelloWorld]
  END
```

---

## Web UI Feature Walkthrough

### Panel Overview

| Panel | Purpose |
|-------|---------|
| **Authentication** | Login / Register / Logout / Exit / Reconnect |
| **Course Browser** | Search courses by code, instructor, or semester |
| **Admin Panel** | Add / Update / Delete courses (Admin only) |
| **Concurrency Tasks** | Multi-connection demo, stress test, sequential test |
| **Protocol Demo** | Send raw protocol commands, view server responses |
| **Encryption Demo** | XOR cipher encryption/decryption demonstration |
| **Server Status** | Display server configuration and statistics |
| **System Log** | Real-time operation log with timestamps |

### Quick Navigation Bar
Buttons at the top allow instant scrolling to any panel.

### Connection Status Indicator
- Green dot = Connected
- Red dot = Disconnected (after Exit)
- Reconnect button appears after Exit to restore connection

---






