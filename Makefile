# Makefile for Course Timetable System
# Compiler
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -I.
LDFLAGS = -lws2_32

# Directories
SERVER_SRC = server/main.cpp server/server.cpp server/client_handler.cpp \
             server/protocol.cpp server/logger.cpp server/auth.cpp \
             database/course_db.cpp
SERVER_OUT = timetable_server.exe

all: $(SERVER_OUT)

$(SERVER_OUT): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) $(SERVER_SRC) -o $(SERVER_OUT) $(LDFLAGS)

clean:
	del /Q $(SERVER_OUT) 2>nul || true

.PHONY: all clean
