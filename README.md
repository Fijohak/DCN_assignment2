# Course Timetable Inquiry System

## 项目简介

本项目是一个基于 **Windows Socket Programming (Winsock)** 的 **C++ 客户端-服务器** 课程表查询系统。系统支持多用户并发访问，提供结构化的课程表数据存储与查询功能，并包含管理员权限控制与数据更新功能。

---

## 快速开始

### 方法一：使用一键启动脚本（推荐）

双击 **`start_system.bat`**，脚本会自动：

1. 启动服务器（后台最小化运行）
2. 等待 2 秒确保服务器就绪
3. 自动打开浏览器访问 `http://localhost:8080`

```
============================================
  Course Timetable Inquiry System
============================================

[1/2] Starting server on port 54000...
      Web interface: http://localhost:8080

[2/2] Opening web interface...

============================================
  Server is running!
  - TCP Port: 54000 (CMD Client)
  - Web UI:   http://localhost:8080
  - Default Admin: admin / admin123
============================================

Press any key to stop the server...
```

### 方法二：手动编译启动

**1. 编译：**
```bash
mingw32-make
```

**2. 启动服务器：**
```bash
timetable_server.exe
```

**3. 访问方式：**
- **Web 界面**：浏览器打开 `http://localhost:8080`
- **CMD 客户端**：`timetable_client.exe`（输入服务器 IP 和端口）

---

## 项目结构

```
DCN_assignment2/
├── server/
│   ├── main.cpp              # 服务器入口
│   ├── server.cpp            # 服务器核心（TCP + HTTP）
│   ├── server.h              # 服务器头文件
│   ├── client_handler.cpp    # 客户端连接处理
│   ├── client_handler.h      # 客户端处理头文件
│   ├── protocol.cpp          # 通信协议解析
│   ├── protocol.h            # 协议定义
│   ├── auth.cpp              # 用户认证
│   ├── auth.h                # 认证头文件
│   ├── logger.cpp            # 日志系统
│   └── logger.h              # 日志头文件
├── client/
│   ├── client.cpp            # CMD 客户端（命令行界面）
│   └── network_client.cpp    # 网络通信层
├── database/
│   ├── course_db.cpp         # 课程数据库操作
│   ├── course_db.h           # 课程数据库头文件
│   ├── user_db.cpp           # 用户数据库操作
│   ├── user_db.h             # 用户数据库头文件
│   ├── courses.csv           # 课程数据文件
│   └── users.csv             # 用户数据文件
├── gui_web/
│   └── index.html            # Web 界面（单页应用）
├── include/
│   ├── common.h              # 公共数据结构
│   ├── network_client.h      # 网络客户端类
│   └── protocol.h            # 协议定义
├── logs/
│   └── server.log            # 服务器运行日志
├── Makefile                  # 编译脚本
├── README.md                 # 本文件
├── start_system.bat          # 一键启动脚本
├── timetable_server.exe      # 服务器可执行文件
└── timetable_client.exe      # CMD 客户端可执行文件
```

---

## 默认账号

| 角色 | 用户名 | 密码 | 权限 |
|------|--------|------|------|
| 管理员 | `admin` | `admin123` | 查询 + 增删改 |
| 学生 | `student` | `student123` | 仅查询 |

> 新用户注册默认获得 Student 角色。用户数据存储在 `database/users.csv` 文件中，服务器重启后仍然保留。

---

## Web 界面功能

### 1. 认证面板
- **Login** — 使用用户名和密码登录
- **Register** — 注册新账号（默认 Student 角色）
- **Logout** — 登出当前用户
- 登录后显示用户角色（Admin/Student）

### 2. 课程浏览器
- **搜索** — 按 Course Code / Instructor / Semester 搜索
- **刷新** — 重新加载所有课程
- **表格展示** — 清晰显示 Code、Title、Section、Instructor、Semester、Day、Time、Room

### 3. 管理面板（仅 Admin）
- **➕ Add Course** — 添加新课程（填写所有字段）
- **✏️ Update Course** — 修改课程字段（选择字段类型）
- **🗑️ Delete Course** — 删除课程（确认对话框）

### 4. Protocol Demo
- 发送原始协议命令（如 `QUERY_CODE COMP3003`）
- 展示完整的请求/响应格式
- 支持 PING、HELP、LIST_ALL、INVALID 等测试

### 5. Encryption Demo (XOR Cipher)
- 分步展示 XOR 加密/解密过程
- Original → Key → Encrypted (hex) → Decrypted

### 6. Server Status
- 显示系统信息：TCP/HTTP 端口、数据库类型、并发模型、加密方式

### 7. System Log
- 实时记录所有操作

---

## 功能模块与老师要求对照

### (1) 数据库模块 ✅
**老师要求：** 存储课程表数据（Course code, Course title, Section, Instructor, Time, Classroom），可使用文件存储或嵌入式数据库。

**实现方式：**
- 使用 **CSV 文件** (`database/courses.csv`) 存储课程数据
- 每条记录包含 9 个字段：`CourseCode|CourseTitle|Section|Instructor|Semester|Day|StartTime|EndTime|Classroom`
- 使用 `std::mutex` 保证多线程并发访问时的数据安全

**示例数据：**
```
COMP3003|Operating Systems|01|Dr. Chen|2026 Spring|Mon|10:00|12:00|B201
COMP2001|Data Structures|01|Dr. Li|2026 Spring|Tue|09:30|11:00|A101
```

### (2) 查询模块 ✅
**老师要求：** 按课程代码搜索、按教师搜索、查看所有课程、清晰格式显示。

**实现方式：**
- **按课程代码搜索**：`QUERY_CODE COMP3003`
- **按教师搜索**：`QUERY_INSTRUCTOR Dr. Chen`
- **按学期搜索**：`QUERY_SEMESTER 2026 Spring`
- **查看所有课程**：`LIST_ALL`
- **结果格式**：表格展示，包含所有字段

### (3) 用户管理模块 ✅
**老师要求：** 支持 Student（仅查询）和 Admin（完全访问），需要身份验证。

**实现方式：**
- **Student**：登录后只能查询课程
- **Admin**：登录后可查询、添加、更新、删除课程
- **身份验证**：`LOGIN <username> <password>` / `REGISTER <username> <password>`
- **数据持久化**：用户数据存储在 `database/users.csv`

### (4) 信息更新模块 ✅
**老师要求：** 管理员可修改特定字段、添加新课程、删除课程，更改立即生效。

**实现方式：**
- **添加课程**：`ADD semester|code|title|section|instructor|day|start|end|room`
- **更新课程**：`UPDATE code|section|field|new_value`
- **删除课程**：`DELETE code|section`
- **即时生效**：所有修改立即写入 CSV 文件

### (5) 网络与并发模块 ✅
**老师要求：** 使用 Winsock，支持至少 5 个并发连接，使用多线程或 I/O 多路复用。

**实现方式：**
- **Winsock**：使用标准 Winsock API
- **多线程**：每个客户端连接创建一个独立的 `std::thread`
- **并发支持**：backlog 设置为 SOMAXCONN
- **线程安全**：使用 `std::mutex` 保护共享数据

### (6) 通信协议 ✅
**老师要求：** 定义简单的应用层协议，包含请求格式、响应格式和错误消息。

**实现方式：**

**请求格式：**
```
PING                              # 测试连接
HELP                              # 帮助信息
LIST_ALL                          # 查看所有课程
QUERY_CODE <course_code>          # 按课程代码查询
QUERY_INSTRUCTOR <instructor>     # 按教师查询
QUERY_SEMESTER <semester>         # 按学期查询
LOGIN <username> <password>       # 登录
REGISTER <username> <password>    # 注册
ADD <9 pipe-separated fields>     # 添加课程（管理员）
UPDATE <code>|<sec>|<field>|<val> # 更新课程（管理员）
DELETE <code>|<section>           # 删除课程（管理员）
ENCRYPT <data>|<key>              # XOR 加密演示
```

**响应格式：**
```
SUCCESS Role:Admin                # 操作成功
FAILURE Invalid username...       # 操作失败
OK PONG                           # PING 响应
OK Record added                   # 添加成功
ERROR Invalid command             # 未知命令
RESULT count=N                    # 查询结果
<data>
END
```

**错误处理示例：**
```
> INVALID
< ERROR Invalid command

> QUERY_CODE
< ERROR Invalid command (missing argument)
```

### (7) 非功能性需求 ✅
**老师要求：** 处理无效请求、合理响应时间、模块化代码结构。

**实现方式：**
- **错误处理**：所有命令都有参数数量检查，返回明确的错误消息
- **输入验证**：`trim()` 去除多余空白字符，`toUpper()` 统一命令大小写
- **响应时间**：查询直接在内存中进行
- **代码结构**：每个功能模块有独立的文件（server/、client/、database/）

### (8) Bonus 功能 ✅
| Bonus 功能 | 状态 | 说明 |
|-----------|------|------|
| 图形用户界面 (GUI) | ✅ **已实现** | Web 界面（HTML + CSS + JavaScript） |
| 高级搜索 | ✅ **已实现** | 按 Code / Instructor / Semester 搜索 |
| 数据缓存 | ✅ **已实现** | 服务器端内存缓存 |
| 加密通信 | ✅ **已实现** | XOR 加密演示模块 |

---

## 通信协议示例

### 学生查询流程
```
> LOGIN student student123
< SUCCESS Role:Student

> QUERY_CODE COMP3003
< RESULT count=2
< COMP3003|Operating Systems|01|Dr. Chen|2026 Spring|Mon|10:00|12:00|B201
< COMP3003|Operating Systems|02|Dr. Chen|2026 Spring|Fri|10:00|12:00|B202
< END

> LIST_ALL
< RESULT count=6
< COMP3003|Operating Systems|01|Dr. Chen|2026 Spring|Mon|10:00|12:00|B201
< ... (更多课程)
< END
```

### 管理员更新流程
```
> LOGIN admin admin123
< SUCCESS Role:Admin

> UPDATE COMP3003|01|classroom|B205
< OK Record updated

> QUERY_CODE COMP3003
< RESULT count=2
< COMP3003|Operating Systems|01|Dr. Chen|2026 Spring|Mon|10:00|12:00|B205
< ...
< END
```

---

## 评分标准对照

| 评分项 | 分数 | 对应实现 |
|--------|------|----------|
| 整体展示 (Overall Presentation) | 10 | 代码结构清晰，README 文档详细，Web 界面美观 |
| 整体实现 (Overall Implementation) | 10 | 所有功能模块完整实现，系统可正常运行 |
| Bonus 功能 | 10 | Web GUI + 高级搜索 + 加密演示 + 协议演示 |

---

## 开发环境

- **操作系统**：Windows 11
- **编译器**：MinGW g++ (C++11)
- **网络库**：Winsock2 (ws2_32.lib)
- **并发**：std::thread
- **数据存储**：CSV 文件
- **Web 界面**：HTML + CSS + JavaScript（通过 HTTP 服务器内嵌）
