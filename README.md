# Course Timetable Inquiry System

## 项目简介

本项目是一个基于 **Windows Socket Programming (Winsock)** 的 **C++ 客户端-服务器** 课程表查询系统。系统支持多用户并发访问，提供结构化的课程表数据存储与查询功能，并包含管理员权限控制与数据更新功能。

---

## 项目结构

```
As_2/
├── include/
│   ├── common.h                  # 公共数据结构定义
│   └── protocol.h                # 通信协议定义 + 加密模块
├── server/
│   └── server.cpp                 # 服务器端（单文件，约550行）
├── client/
│   ├── client.cpp                 # CMD 客户端（命令行界面）
│   └── gui_client.cpp             # GUI 客户端（图形界面，Win32 API）
├── data/
│   ├── timetable.csv              # 课程数据文件（CSV格式）
│   ├── users.csv                  # 用户数据文件（CSV格式）
│   └── server.log                 # 服务器运行日志
├── Makefile                       # 编译脚本
├── README.md                      # 本文件
├── start_system.bat               # 一键启动脚本
├── test_client.bat                # 测试批处理文件
├── timetable_server.exe           # 服务器可执行文件
├── timetable_client.exe           # CMD 客户端可执行文件
└── timetable_gui.exe              # GUI 客户端可执行文件
```

---

## 快速开始

### 方法一：使用一键启动脚本（推荐）

双击 **`start_system.bat`**，选择客户端类型：

```
Choose client type:
  1. CMD Client (console)    ← 命令行界面
  2. GUI Client (graphical)  ← 图形界面
Enter choice (1 or 2):
```

脚本会自动：
1. 检查可执行文件是否存在
2. 启动服务器（最小化窗口）
3. 等待 2 秒确保服务器就绪
4. 启动客户端
5. 关闭客户端后按任意键自动关闭服务器

### 方法二：手动启动

**1. 编译（如无可执行文件）：**
```bash
# 编译服务器
g++ -std=c++11 -Wall -Wextra server/server.cpp -o timetable_server.exe -lws2_32

# 编译 CMD 客户端
g++ -std=c++11 -Wall -Wextra client/client.cpp -o timetable_client.exe -lws2_32

# 编译 GUI 客户端
g++ -std=c++11 -Wall client/gui_client.cpp -o timetable_gui.exe -lws2_32 -lgdi32 -lcomctl32 -mwindows
```

或使用 Makefile：
```bash
make
```

**2. 启动服务器：**
```bash
timetable_server.exe
```
服务器默认监听端口 **8888**，支持最多 **10** 个并发客户端连接。

**3. 启动客户端：**
- **CMD 客户端**：`timetable_client.exe`（输入服务器 IP 和端口）
- **GUI 客户端**：`timetable_gui.exe`（在界面中输入服务器地址）

---

## 默认账号

| 角色 | 用户名 | 密码 | 权限 |
|------|--------|------|------|
| 学生 | `student` | `student123` | 仅查询 |
| 管理员 | `admin` | `admin123` | 查询 + 增删改 |

> **新用户注册**：在客户端主菜单中选择 "2. Register" 即可注册新账号（默认注册为学生角色）。用户数据存储在 `data/users.csv` 文件中，服务器重启后仍然保留。

---

## 使用流程

### CMD 客户端使用流程

```
=== Course Timetable Client ===
Server IP (default: 127.0.0.1):          ← 回车使用默认
Port (default: 8888):                    ← 回车使用默认

--- Main Menu ---
1. Login                                  ← 登录
2. Register                               ← 注册新账号
3. Exit                                   ← 退出
Choice: 1
Username: student
Password: student123
SUCCESS Login successful. Role: Student
Logged in as Student

--- Student Menu ---
1. Search by Course Code                  ← 按课程代码查询
2. Search by Instructor                   ← 按教师查询
3. View All Courses                       ← 查看全部课程
4. Logout                                 ← 登出
Choice: 1
Course Code: COMP3003

COMP3003 | Data Communications and Networking | Sec 2 | Dr. Johnson | Wed-14:00-16:00 | Room 302
```

### GUI 客户端使用流程

**1. 连接服务器：**
- 在 "Server" 输入框输入 `127.0.0.1:8888`
- 点击 **Connect** 按钮

**2. 登录/注册：**
- 在 "Search" 输入框输入 `student student123`
- 点击 **Login** 按钮
- 或输入新用户名密码后点击 **Register**

**3. 查询课程：**
- 在 "Search" 输入框输入课程代码（如 `COMP3003`），点击 **By Code**
- 或输入教师姓名（如 `Dr. Johnson`），点击 **By Instructor**
- 或直接点击 **View All** 查看全部课程
- **高级搜索**：输入关键词后点击 **By Time** / **By Title** / **By Room**

**4. 管理员操作（需以 admin 登录）：**
- 在下方 "Admin Panel" 输入框中填写课程信息
- 点击 **Add** 添加课程
- 选择字段（title/instructor/time/classroom），输入新值，点击 **Update**
- 点击 **Delete** 删除课程（会弹出确认对话框）

---

## 功能模块与老师要求对照

### (1) 数据库模块 ✅

**老师要求：**
- 存储课程表数据：课程代码、课程名称、Section、教师、时间（天和时长）、教室
- 可使用文件存储（CSV、TXT）或嵌入式数据库（SQLite）

**实现方式：**
- 使用 **CSV 文件** (`data/timetable.csv`) 存储课程数据
- 每条记录包含 6 个字段：`CourseCode, CourseTitle, Section, Instructor, Time, Classroom`
- 服务器启动时从 CSV 加载数据到内存，修改后自动保存回 CSV
- 使用 `std::mutex` 保证多线程并发访问时的数据安全

**示例数据：**
```
CourseCode,CourseTitle,Section,Instructor,Time,Classroom
COMP3003,Data Communications and Networking,2,Dr. Johnson,Wed-14:00-16:00,Room 302
COMP2002,Operating Systems,1,Prof. Lee,Tue-09:00-11:00,Room 201
```

**对应代码位置：** `server/server.cpp` 中的 `loadDatabase()` 和 `saveDatabase()` 函数

---

### (2) 查询模块（客户端） ✅

**老师要求：**
- 按课程代码搜索
- 按教师搜索
- 查看某学期的所有课程
- 以清晰可读的格式显示结果

**实现方式：**
- **按课程代码搜索**：输入课程代码（如 `COMP3003`），返回匹配的所有 Section
- **按教师搜索**：输入教师姓名（支持部分匹配），返回该教师的所有课程
- **查看所有课程**：显示数据库中全部课程记录
- **结果格式**：`课程代码 | 课程名称 | Sec X | 教师 | 时间 | 教室`

**CMD 客户端操作：**
```
--- Student Menu ---
1. Search by Course Code    ← 输入课程代码
2. Search by Instructor     ← 输入教师姓名
3. View All Courses         ← 查看全部
4. Logout
Choice: 1
Course Code: COMP3003

COMP3003 | Data Communications and Networking | Sec 2 | Dr. Johnson | Wed-14:00-16:00 | Room 302
```

**GUI 客户端操作：**
- 在 Search 输入框输入关键词
- 点击 **By Code** / **By Instructor** / **View All** 按钮
- 结果在下方列表框中显示

**对应代码位置：**
- CMD 客户端：`client/client.cpp` 中的菜单循环和 `displayResults()` 函数
- GUI 客户端：`client/gui_client.cpp` 中的 `ID_BTN_SEARCH_CODE`、`ID_BTN_SEARCH_INST`、`ID_BTN_VIEW_ALL` 处理逻辑
- 服务器：`server/server.cpp` 中的 `searchByCode()`、`searchByInstructor()`、`viewAll()` 函数

---

### (3) 用户管理模块 ✅

**老师要求：**
- 支持至少两种角色：学生（默认，仅查询）和管理员（完全访问）
- 管理员功能：添加、更新、删除课程记录
- 管理员访问需要身份验证（用户名/密码）

**实现方式：**
- **两种角色**：
  - `Student`（学生）：登录后只能查询课程
  - `Admin`（管理员）：登录后可查询、添加、更新、删除课程
- **身份验证**：使用用户名和密码登录，服务器验证后返回角色信息
- **会话管理**：服务器端维护登录状态，未登录用户无法执行任何操作
- **数据持久化**：用户数据存储在 `data/users.csv`，服务器重启后保留
- **用户注册**：新用户可自行注册账号（默认学生角色）

**CMD 客户端登录流程：**
```
--- Main Menu ---
1. Login
2. Register             ← 注册新账号
3. Exit
Choice: 1
Username: admin
Password: admin123
SUCCESS Login successful. Role: Admin
Logged in as Administrator
```

**GUI 客户端登录流程：**
1. 在 Search 输入框输入 `admin admin123`
2. 点击 **Login** 按钮
3. 状态栏显示 "Logged in as admin (Admin)"
4. 管理员按钮（Add/Update/Delete）自动启用

**对应代码位置：**
- 数据结构：`include/common.h` 中的 `UserRole` 枚举
- 用户验证：`server/server.cpp` 中的 `loadUsers()`、`saveUsers()`、`loginUser()`、`registerUser()` 函数
- 权限检查：`server/server.cpp` 中每个命令处理前的 `if (!loggedIn || !isAdmin)` 检查

---

### (4) 信息更新模块 ✅

**老师要求：**
- 管理员可修改特定字段（如时间或教室）
- 添加新的课程条目
- 删除过时的条目
- 更改必须立即对所有连接的客户端生效

**实现方式：**
- **添加课程**：`ADD <code> <title> <section> <instructor> <time> <classroom>`
- **更新课程**：`UPDATE <code> <section> <field> <newvalue>`（支持 title/instructor/time/classroom）
- **删除课程**：`DELETE <code> <section>`
- **即时生效**：所有修改立即写入 CSV 文件，后续查询直接从内存读取最新数据

**CMD 客户端管理员操作：**
```
--- Admin Menu ---
1. Search by Course Code
2. Search by Instructor
3. View All Courses
4. Add Course              ← 添加新课程
5. Update Course           ← 更新课程信息
6. Delete Course           ← 删除课程
7. Logout
Choice: 4
Course Code: TEST1001
Title: Test Course
Section: 1
Instructor: Dr. Test
Time: Mon-10:00-12:00
Classroom: Room 999
SUCCESS Course added
```

**GUI 客户端管理员操作：**
- **添加**：在 Admin Panel 输入框中填写 Code/Title/Sec/Instructor/Time/Room，点击 **Add**
- **更新**：输入 Code 和 Sec，选择字段（下拉框），输入新值，点击 **Update**
- **删除**：输入 Code 和 Sec，点击 **Delete**（弹出确认对话框）

**对应代码位置：** `server/server.cpp` 中的 `handleAdd()`、`handleUpdate()`、`handleDelete()` 逻辑

---

### (5) 网络与并发模块 ✅

**老师要求：**
- 使用 Windows Sockets (Winsock) 实现
- 支持至少 5 个并发客户端连接
- 使用多线程（如 std::thread）或 I/O 多路复用（如 select()）

**实现方式：**
- **Winsock**：使用 `WSAStartup`、`socket`、`bind`、`listen`、`accept` 等标准 Winsock API
- **多线程**：每个客户端连接创建一个独立的 `std::thread` 处理
- **并发支持**：服务器 `listen` 的 backlog 设置为 10，支持多个客户端同时连接
- **线程安全**：使用 `std::mutex` 保护共享数据（课程列表和用户数据）

**服务器启动输出：**
```
=== Course Timetable Server ===

=== Communication Protocol v1.0 ===
Transport: TCP, Encoding: ASCII text
--- Commands ---
  LOGIN <user> <pass>       - Authenticate
  REGISTER <user> <pass>    - Register new user
  LOGOUT                    - End session
  QUERY CODE <code>         - Search by course code
  QUERY INSTRUCTOR <name>   - Search by instructor
  QUERY ALL                 - View all courses
  QUERY TIME <keyword>      - Search by time (Bonus)
  QUERY TITLE <keyword>     - Search by title (Bonus)
  QUERY CLASSROOM <room>    - Search by room (Bonus)
  QUERY ADVANCED <f> <op> <v> - Advanced search (Bonus)
  ADD <code> <title> ...    - Add course (Admin)
  UPDATE <code> <sec> ...   - Update course (Admin)
  DELETE <code> <sec>       - Delete course (Admin)
--- Responses ---
  SUCCESS <msg>             - Operation succeeded
  FAILURE <msg>             - Operation failed
  RESULT\n<data>\nEND       - Query result
  ERROR <msg>               - Unknown command
================================
[2026-05-06 17:24:48] Loaded 10 courses from database
[2026-05-06 17:24:48] Loaded 5 users from database
Server running on port 8888
Press Ctrl+C to stop
```

**对应代码位置：** `server/server.cpp` 中的 `main()` 函数（Winsock 初始化、socket 创建、accept 循环）和 `handleClient()` 函数（每个线程执行）

---

### (6) 通信协议 ✅

**老师要求：**
- 定义简单的应用层协议
- 包含请求格式、响应格式和错误消息

**实现方式：**

**请求格式：**
```
LOGIN <username> <password>       # 登录
LOGOUT                             # 登出
REGISTER <username> <password>     # 注册新用户
QUERY CODE <course_code>           # 按课程代码查询
QUERY INSTRUCTOR <instructor_name> # 按教师查询
QUERY ALL                          # 查看所有课程
QUERY TIME <keyword>               # 按时间搜索 (Bonus)
QUERY TITLE <keyword>              # 按标题搜索 (Bonus)
QUERY CLASSROOM <room>             # 按教室搜索 (Bonus)
QUERY ADVANCED <f> <op> <v>       # 高级搜索 (Bonus)
ADD <code> <title> <sec> <inst> <time> <room>  # 添加课程（管理员）
UPDATE <code> <sec> <field> <val>  # 更新课程（管理员）
DELETE <code> <sec>                # 删除课程（管理员）
```

**响应格式：**
```
SUCCESS <message>\n                # 操作成功
FAILURE <reason>\n                 # 操作失败
ERROR <message>\n                  # 未知命令
RESULT\n<data>END\n                # 查询结果（多行数据）
```

**错误处理示例：**
```
> INVALID COMMAND
ERROR Unknown command

> QUERY CODE
FAILURE Please login first

> ADD test course
FAILURE Admin privileges required
```

**对应代码位置：** `include/protocol.h`（协议定义），`server/server.cpp` 中的命令解析和分发逻辑

---

### (7) 非功能性需求 ✅

**老师要求：**
- 处理无效或格式错误的请求，防止崩溃
- 合理的响应时间
- 清晰模块化的代码结构

**实现方式：**
- **错误处理**：所有命令都有参数数量检查，返回明确的错误消息
- **输入验证**：服务器端 `trim()` 去除多余空白字符，`toupper()` 统一命令大小写
- **响应时间**：查询直接在内存中进行，无需磁盘 I/O
- **代码结构**：每个功能模块有独立的函数，代码注释清晰

---

### (8) Bonus 功能 ✅ 全部实现

**老师列出的 Bonus 功能：**

| Bonus 功能 | 状态 | 说明 |
|-----------|------|------|
| 图形用户界面 (GUI) | ✅ **已实现** | Win32 API 图形界面客户端，支持窗口自适应 |
| 高级搜索 | ✅ **已实现** | 按时间、按标题、按教室、通用高级搜索 |
| 数据缓存 | ✅ **已实现** | 服务器端 30 秒缓存 + 客户端 15 秒缓存 |
| 加密通信 | ✅ **已实现** | XOR 旋转密钥加密 + Hex 编码传输 |

---

## Bonus 功能详解

### 1. 高级搜索（Advanced Search）

**GUI 客户端新增按钮：**
- **By Time** - 在搜索框输入时间/星期关键词（如 `Mon`、`10:00`）
- **By Title** - 在搜索框输入课程标题关键词（如 `Programming`）
- **By Room** - 在搜索框输入教室关键词（如 `Room 10`）

**通用高级搜索命令（CMD 客户端）：**
```
QUERY ADVANCED <field> <operator> <value>
```

**支持的字段：** `code`, `title`, `section`, `instructor`, `time`, `classroom`

**支持的操作符：**
| 操作符 | 说明 | 示例 |
|--------|------|------|
| `=` 或 `eq` | 精确匹配 | `QUERY ADVANCED code = COMP3003` |
| `~=` 或 `contains` | 包含（模糊搜索） | `QUERY ADVANCED title ~= Data` |
| `!=` 或 `ne` | 不等于 | `QUERY ADVANCED instructor != Dr.` |
| `^=` 或 `startswith` | 以...开头 | `QUERY ADVANCED code ^= COMP` |

**对应代码位置：**
- 服务器：`server/server.cpp` 中的 `searchByTime()`、`searchByTitle()`、`searchByClassroom()`、`searchAdvanced()` 函数
- 客户端：`client/gui_client.cpp` 中的 `ID_BTN_SEARCH_TIME`、`ID_BTN_SEARCH_TITLE`、`ID_BTN_SEARCH_ROOM` 处理逻辑

---

### 2. 数据缓存（Data Caching）

**服务器端缓存（`server/server.cpp`）：**
| 特性 | 说明 |
|------|------|
| 缓存结构 | `std::map<std::string, CacheEntry>` 存储查询结果 |
| 有效期 | 30 秒（`CACHE_TTL = 30`） |
| 缓存命中 | 日志输出 `Cache HIT for: ...` |
| 缓存未命中 | 日志输出 `Cache MISS for: ...`，执行查询后存入缓存 |
| 缓存失效 | ADD/UPDATE/DELETE 操作后自动清除所有缓存 |
| 线程安全 | 使用独立的 `g_cacheMutex` 互斥锁保护 |

**客户端缓存（`client/gui_client.cpp`）：**
| 特性 | 说明 |
|------|------|
| 缓存结构 | `std::map<std::string, ClientCacheEntry>` |
| 有效期 | 15 秒（`CLIENT_CACHE_TTL = 15`） |
| 应用范围 | By Code、By Instructor 查询使用客户端缓存 |
| 状态提示 | 缓存命中时显示 `[Cached]` 标记 |

**工作流程：**
1. 客户端首次查询 → 发送请求到服务器 → 服务器查询数据库 → 返回结果并缓存
2. 客户端再次查询（15秒内）→ 直接使用客户端缓存，不发送网络请求
3. 其他客户端查询相同内容（30秒内）→ 服务器直接返回缓存结果
4. 管理员修改数据 → 服务器自动清除所有缓存，确保数据一致性

---

### 3. 加密通信（Secure Communication）

**加密方案：XOR 旋转密钥 + Hex 编码**

**加密流程：**
```
客户端 → 服务器: ENCRYPT 1          (协商加密)
服务器 → 客户端: ENCRYPT_OK          (确认加密)
客户端 → 服务器: <hex_encoded_xor>   (加密后的请求)
服务器 → 客户端: <hex_encoded_xor>   (加密后的响应)
```

**技术细节：**
| 组件 | 说明 |
|------|------|
| 加密算法 | XOR 旋转密钥（`"TIMETABLE2024"`，12字节） |
| 传输编码 | Hex 十六进制编码（确保 TCP 安全传输） |
| 对称性 | XOR 是对称加密，加密=解密 |
| 密钥轮转 | `key[i % key_length]` 防止简单频率分析 |

**代码实现位置：**
- `include/protocol.h` - `encryptMessage()` / `decryptMessage()` / `isEncrypted()` 函数
- `server/server.cpp` - `handleClient()` 中的加密协商和解密逻辑
- `client/gui_client.cpp` - `connectToServer()` 中的加密协商，`sendRequest()` 中的加解密

**演示效果：**
- 连接后自动协商加密
- 服务器日志显示 `Encryption enabled for this session`
- 所有后续通信（LOGIN 密码、QUERY 请求、课程数据）均加密传输
- 网络抓包看到的是 hex 编码的密文，而非明文

---

## 通信协议示例

### 学生查询流程
```
客户端 → 服务器: LOGIN student student123
服务器 → 客户端: SUCCESS Login successful. Role: Student

客户端 → 服务器: QUERY CODE COMP3003
服务器 → 客户端: RESULT
COMP3003 | Data Communications and Networking | Sec 2 | Dr. Johnson | Wed-14:00-16:00 | Room 302
END

客户端 → 服务器: QUERY ALL
服务器 → 客户端: RESULT
COMP3003 | Data Communications and Networking | Sec 2 | Dr. Johnson | Wed-14:00-16:00 | Room 302
COMP2002 | Operating Systems | Sec 1 | Prof. Lee | Tue-09:00-11:00 | Room 201
...（更多课程）
END

客户端 → 服务器: LOGOUT
服务器 → 客户端: SUCCESS Logged out
```

### 管理员更新流程
```
客户端 → 服务器: LOGIN admin admin123
服务器 → 客户端: SUCCESS Login successful. Role: Admin

客户端 → 服务器: UPDATE COMP3003 2 time Fri-10:00-12:00
服务器 → 客户端: SUCCESS Course updated

客户端 → 服务器: QUERY CODE COMP3003
服务器 → 客户端: RESULT
COMP3003 | Data Communications and Networking | Sec 2 | Dr. Johnson | Fri-10:00-12:00 | Room 302
END
```

### 用户注册流程
```
客户端 → 服务器: REGISTER newuser pass123
服务器 → 客户端: SUCCESS Registration successful. You can now login.

客户端 → 服务器: LOGIN newuser pass123
服务器 → 客户端: SUCCESS Login successful. Role: Student
```

### 加密通信流程
```
客户端 → 服务器: ENCRYPT 1
服务器 → 客户端: ENCRYPT_OK
客户端 → 服务器: 4a1f3c2b... (加密后的 LOGIN admin admin123)
服务器 → 客户端: 5e2a1b3c... (加密后的 SUCCESS Login successful)
```

---

## 评分标准对照

| 评分项 | 分数 | 对应实现 |
|--------|------|----------|
| 整体展示 (Overall Presentation) | 10 | 代码结构清晰，注释完整，README 文档详细 |
| 整体实现 (Overall Implementation) | 10 | 所有功能模块完整实现，系统可正常运行 |
| Bonus 功能 | 10 | GUI 客户端 + 高级搜索 + 数据缓存 + 加密通信 |

---

## 开发环境

- **操作系统**：Windows 11
- **编译器**：MinGW g++ (C++11)
- **网络库**：Winsock2 (ws2_32.lib)
- **并发**：std::thread
- **数据存储**：CSV 文件
- **GUI 库**：Win32 API (gdi32, comctl32)
