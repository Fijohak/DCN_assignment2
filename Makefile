# Makefile for Course Timetable System
# Compiler
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
LDFLAGS = -lws2_32
GUI_LDFLAGS = -lws2_32 -lgdi32 -lcomctl32 -mwindows

# Directories
SERVER_SRC = server/server.cpp
CLIENT_SRC = client/client.cpp
GUI_CLIENT_SRC = client/gui_client.cpp
SERVER_OUT = timetable_server.exe
CLIENT_OUT = timetable_client.exe
GUI_CLIENT_OUT = timetable_gui.exe

all: $(SERVER_OUT) $(CLIENT_OUT) $(GUI_CLIENT_OUT)

$(SERVER_OUT): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) $(SERVER_SRC) -o $(SERVER_OUT) $(LDFLAGS)

$(CLIENT_OUT): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) $(CLIENT_SRC) -o $(CLIENT_OUT) $(LDFLAGS)

$(GUI_CLIENT_OUT): $(GUI_CLIENT_SRC)
	$(CXX) $(CXXFLAGS) $(GUI_CLIENT_SRC) -o $(GUI_CLIENT_OUT) $(GUI_LDFLAGS)

clean:
	del /Q $(SERVER_OUT) $(CLIENT_OUT) $(GUI_CLIENT_OUT) 2>nul || true

.PHONY: all clean
