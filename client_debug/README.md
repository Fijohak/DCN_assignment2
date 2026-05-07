# Debug Client

这是一个简易 TCP 调试客户端，用来手动测试 `server/` 里的课程时间表服务器。

## Windows Visual Studio

把 `client_debug/debug_client.cpp` 单独建成一个 Console App，并链接：

```txt
ws2_32.lib
```

运行后默认连接：

```txt
127.0.0.1:54000
```

也可以指定 host 和 port：

```txt
debug_client.exe 127.0.0.1 54000
```

## 命令示例

```txt
PING
HELP
LIST_ALL
QUERY_CODE COMP3003
QUERY_INSTRUCTOR Dr. Chen
QUERY_SEMESTER 2026 Spring
LOGIN admin password123
UPDATE COMP3003|01|CLASSROOM|B301
QUIT
```

## 使用顺序

1. 先启动 server。
2. 再启动 debug client。
3. 在 debug client 中输入命令并查看服务器响应。
