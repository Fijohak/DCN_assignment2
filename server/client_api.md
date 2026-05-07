# 课程时间表服务器客户端 API

## 连接信息

- 主机地址：`127.0.0.1`
- 端口：`54000`
- 传输协议：TCP
- 编码格式：纯文本
- 请求结束符：换行符 `\n`

客户端每发送一条请求，服务器都会返回一个完整响应。查询类响应统一以
`RESULT count=N` 开头，并以 `END` 结束。

## 学生端命令

```txt
PING
HELP
LIST_ALL
QUERY_CODE <course_code>
QUERY_INSTRUCTOR <instructor_name>
QUERY_SEMESTER <semester>
QUIT
```

示例：

```txt
QUERY_CODE COMP3003
QUERY_INSTRUCTOR Dr. Chen
QUERY_SEMESTER 2026 Spring
```

查询响应格式：

```txt
RESULT count=2
COMP3003|Operating Systems|01|Dr. Chen|2026 Spring|Mon|10:00|12:00|B201
COMP3003|Operating Systems|02|Dr. Chen|2026 Spring|Fri|10:00|12:00|B202
END
```

课程字段始终按以下顺序返回：

```txt
course_code|course_title|section|instructor|semester|day|start_time|end_time|classroom
```

## 管理员命令

管理员必须先登录，才能添加、更新或删除课程记录：

```txt
LOGIN admin password123
```

管理员更新命令：

```txt
ADD <semester>|<course_code>|<course_title>|<section>|<instructor>|<day>|<start_time>|<end_time>|<classroom>
UPDATE <course_code>|<section>|<field>|<new_value>
DELETE <course_code>|<section>
```

示例：

```txt
ADD 2026 Spring|COMP5001|Distributed Systems|01|Dr. Smith|Wed|14:00|16:00|E201
UPDATE COMP3003|01|CLASSROOM|B301
UPDATE COMP3003|01|TIME|Tue,09:00-11:00
DELETE COMP3003|01
```

支持更新的字段：

```txt
CLASSROOM
DAY
START_TIME
END_TIME
INSTRUCTOR
COURSE_TITLE
SEMESTER
TIME
```

如果更新 `TIME` 字段，请使用以下值格式：

```txt
Day,start-end
```

示例：

```txt
UPDATE COMP3003|01|TIME|Tue,09:00-11:00
```

## 常见响应

```txt
OK PONG
SUCCESS Logged in
FAILURE Invalid username or password
OK Record added
OK Record updated
OK Record deleted
ERROR Permission denied
ERROR Invalid command
ERROR Record not found
```

错误响应不会断开客户端连接。只有在客户端发送 `QUIT`、客户端主动断开连接，
或发生网络错误时，连接才会关闭。
