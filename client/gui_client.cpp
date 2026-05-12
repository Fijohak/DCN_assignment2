// Course Timetable System - GUI Client (Win32 API)
// Build: g++ -std=c++11 -Wall client/gui_client.cpp client/network_client.cpp
//        -o timetable_gui.exe -lws2_32 -lgdi32 -lcomctl32 -mwindows

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>
#include <cctype>
#include "../include/protocol.h"
#include "../include/network_client.h"

// ==================== Global State ====================
NetworkClient g_client;
bool g_loggedIn = false;
bool g_isAdmin = false;
std::string g_username;

// ==================== Control IDs ====================
#define ID_BTN_CONNECT        1010
#define ID_BTN_LOGIN          1001
#define ID_BTN_REGISTER       1002
#define ID_BTN_LOGOUT         1003
#define ID_BTN_SEARCH_CODE    1004
#define ID_BTN_SEARCH_INST    1005
#define ID_BTN_VIEW_ALL       1006
#define ID_BTN_ADD            1007
#define ID_BTN_UPDATE         1008
#define ID_BTN_DELETE         1009
#define ID_EDIT_SERVER        2001
#define ID_EDIT_USERNAME      2002
#define ID_EDIT_PASSWORD      2003
#define ID_EDIT_SEARCH        2004
#define ID_EDIT_CODE          2005
#define ID_EDIT_TITLE         2006
#define ID_EDIT_SECTION       2007
#define ID_EDIT_INSTRUCTOR    2008
#define ID_EDIT_TIME          2009
#define ID_EDIT_NEWVAL        2011
#define ID_COMBO_FIELD        4001
#define ID_LIST_RESULT        3001
#define ID_STATIC_STATUS      5001

// ==================== Layout Constants ====================
#define MARGIN 10
#define ROW1_Y 48
#define ROW2_Y 80
#define LIST_Y 112
#define LIST_H 185
#define ADMIN1_Y 305
#define ADMIN2_Y 340
#define STATUS_Y 380
#define CTRL_H 24
#define BTN_H 28
#define LABEL_H 22

// ==================== Helpers ====================

std::string cleanResponse(const std::string& response) {
    std::string display = response;

    size_t pos = display.find(RESULT_START);
    if (pos == 0) {
        pos = display.find('\n');
        if (pos != std::string::npos)
            display = display.substr(pos + 1);
    }

    pos = display.find(RESULT_END);
    if (pos != std::string::npos)
        display = display.substr(0, pos);

    if (display.find(OK_PREFIX) == 0)
        display = display.substr(strlen(OK_PREFIX));
    else if (display.find(SUCCESS_PREFIX) == 0)
        display = display.substr(strlen(SUCCESS_PREFIX));
    else if (display.find(FAILURE_PREFIX) == 0)
        display = display.substr(strlen(FAILURE_PREFIX));
    else if (display.find(ERROR_PREFIX) == 0)
        display = display.substr(strlen(ERROR_PREFIX));

    while (!display.empty() && (display.back() == '\r' || display.back() == '\n'))
        display.pop_back();

    return display;
}

void clearListBox(HWND hLB) { SendMessageA(hLB, LB_RESETCONTENT, 0, 0); }

void appendListBox(HWND hLB, const std::string& text) {
    int idx = SendMessageA(hLB, LB_ADDSTRING, 0, (LPARAM)text.c_str());
    SendMessageA(hLB, LB_SETTOPINDEX, idx, 0);
}

void appendListBoxText(HWND hLB, const std::string& text) {
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) appendListBox(hLB, line);
    }
}

void showQueryResult(HWND hLB, HWND hStatus, const std::string& response,
                     const std::string& header) {
    if (response.find(ERROR_PREFIX) == 0 || response.find(FAILURE_PREFIX) == 0) {
        SetWindowTextA(hStatus, ("Error: " + cleanResponse(response)).c_str());
        appendListBox(hLB, "Error: " + cleanResponse(response));
        return;
    }
    std::string clean = cleanResponse(response);
    clearListBox(hLB);
    appendListBox(hLB, header);
    appendListBoxText(hLB, clean);
    SetWindowTextA(hStatus, header.c_str());
}

// ==================== Window Procedure ====================
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hListBox, hStaticStatus;
    static HWND hEditServer, hEditUsername, hEditPassword, hEditSearch;
    static HWND hEditCode, hEditTitle, hEditSection, hEditInstructor, hEditTime, hEditNewVal;
    static HWND hComboField;
    static HWND hBtnConnect, hBtnLogin, hBtnRegister, hBtnLogout;
    static HWND hBtnSearchCode, hBtnSearchInst, hBtnViewAll;
    static HWND hBtnAdd, hBtnUpdate, hBtnDelete;
    static HWND hTitle;
    static HWND hLabelCode, hLabelTitle, hLabelSec, hLabelInst, hLabelTime;
    static HWND hLabelAdminPanel;
    static HWND hLabelUpdateField, hLabelNewVal;
    static HWND hLabelUser, hLabelPwd;
    static HWND hLabelServer, hLabelSearch;
    static int m = MARGIN;

    switch (msg) {
    case WM_CREATE: {
        HDC hdc = GetDC(hWnd);
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(hWnd, hdc);

        HFONT hFontTitle = CreateFontA(-MulDiv(18, dpi, 72), 0, 0, 0, FW_BOLD,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hFontNormal = CreateFontA(-MulDiv(11, dpi, 72), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hFontBold = CreateFontA(-MulDiv(11, dpi, 72), 0, 0, 0, FW_BOLD,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hFontMono = CreateFontA(-MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");

        // ===== Title =====
        hTitle = CreateWindowA("STATIC", "Course Timetable Inquiry System",
            WS_CHILD | WS_VISIBLE | SS_CENTER, m, 10, 780, 30, hWnd, NULL, NULL, NULL);
        SendMessageA(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

        // ===== Row 1: Server | Connect | User | Pwd | Login | Reg | Logout =====
        int x = m;
        hLabelServer = CreateWindowA("STATIC", "Server:", WS_CHILD | WS_VISIBLE,
            x, ROW1_Y, 50, LABEL_H, hWnd, NULL, NULL, NULL);
        SendMessageA(hLabelServer, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 50;
        hEditServer = CreateWindowA("EDIT", "127.0.0.1:54000",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            x, ROW1_Y - 2, 160, CTRL_H, hWnd, (HMENU)ID_EDIT_SERVER, NULL, NULL);
        SendMessageA(hEditServer, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 165;
        hBtnConnect = CreateWindowA("BUTTON", "Connect",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, ROW1_Y - 2, 75, CTRL_H, hWnd, (HMENU)ID_BTN_CONNECT, NULL, NULL);
        SendMessageA(hBtnConnect, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        x += 85;

        // Login area
        hLabelUser = CreateWindowA("STATIC", "User:", WS_CHILD | WS_VISIBLE,
            x, ROW1_Y, 40, LABEL_H, hWnd, NULL, NULL, NULL);
        SendMessageA(hLabelUser, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 40;
        hEditUsername = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            x, ROW1_Y - 2, 90, CTRL_H, hWnd, (HMENU)ID_EDIT_USERNAME, NULL, NULL);
        SendMessageA(hEditUsername, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 95;
        hLabelPwd = CreateWindowA("STATIC", "Pwd:", WS_CHILD | WS_VISIBLE,
            x, ROW1_Y, 35, LABEL_H, hWnd, NULL, NULL, NULL);
        SendMessageA(hLabelPwd, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 35;
        hEditPassword = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_LEFT,
            x, ROW1_Y - 2, 90, CTRL_H, hWnd, (HMENU)ID_EDIT_PASSWORD, NULL, NULL);
        SendMessageA(hEditPassword, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 95;
        hBtnLogin = CreateWindowA("BUTTON", "Login",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, ROW1_Y - 2, 55, CTRL_H, hWnd, (HMENU)ID_BTN_LOGIN, NULL, NULL);
        SendMessageA(hBtnLogin, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        x += 60;
        hBtnRegister = CreateWindowA("BUTTON", "Reg",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, ROW1_Y - 2, 40, CTRL_H, hWnd, (HMENU)ID_BTN_REGISTER, NULL, NULL);
        SendMessageA(hBtnRegister, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 45;
        hBtnLogout = CreateWindowA("BUTTON", "Logout",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x - 45 - 55, ROW1_Y - 2, 55, CTRL_H, hWnd, (HMENU)ID_BTN_LOGOUT, NULL, NULL);
        SendMessageA(hBtnLogout, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        ShowWindow(hBtnLogout, SW_HIDE);

        // ===== Row 2: Search | Edit | By Code | By Instructor | View All =====
        x = m;
        hLabelSearch = CreateWindowA("STATIC", "Search:", WS_CHILD | WS_VISIBLE,
            x, ROW2_Y, 50, LABEL_H, hWnd, NULL, NULL, NULL);
        SendMessageA(hLabelSearch, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 50;
        hEditSearch = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            x, ROW2_Y - 2, 120, CTRL_H, hWnd, (HMENU)ID_EDIT_SEARCH, NULL, NULL);
        SendMessageA(hEditSearch, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 125;
        hBtnSearchCode = CreateWindowA("BUTTON", "By Code",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, ROW2_Y - 2, 70, CTRL_H, hWnd, (HMENU)ID_BTN_SEARCH_CODE, NULL, NULL);
        SendMessageA(hBtnSearchCode, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 75;
        hBtnSearchInst = CreateWindowA("BUTTON", "By Instructor",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, ROW2_Y - 2, 95, CTRL_H, hWnd, (HMENU)ID_BTN_SEARCH_INST, NULL, NULL);
        SendMessageA(hBtnSearchInst, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 100;
        hBtnViewAll = CreateWindowA("BUTTON", "View All",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, ROW2_Y - 2, 70, CTRL_H, hWnd, (HMENU)ID_BTN_VIEW_ALL, NULL, NULL);
        SendMessageA(hBtnViewAll, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // ===== Results ListBox =====
        hListBox = CreateWindowA("LISTBOX", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT,
            m, LIST_Y, 780, LIST_H, hWnd, (HMENU)ID_LIST_RESULT, NULL, NULL);
        SendMessageA(hListBox, WM_SETFONT, (WPARAM)hFontMono, TRUE);

        // ===== Admin Panel Row 1: Code | Title | Sec | Instructor | Time =====
        x = m;
        hLabelAdminPanel = CreateWindowA("STATIC", "Admin Panel:", WS_CHILD | WS_VISIBLE,
            x, ADMIN1_Y, 100, LABEL_H, hWnd, NULL, NULL, NULL);
        SendMessageA(hLabelAdminPanel, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 105;
        hLabelCode = CreateWindowA("STATIC", "Code:", WS_CHILD | WS_VISIBLE,
            x, ADMIN1_Y, 45, LABEL_H, hWnd, NULL, NULL, NULL);
        SendMessageA(hLabelCode, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 45;
        hEditCode = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            x, ADMIN1_Y - 2, 75, CTRL_H, hWnd, (HMENU)ID_EDIT_CODE, NULL, NULL);
        SendMessageA(hEditCode, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 80;
        hLabelTitle = CreateWindowA("STATIC", "Title:", WS_CHILD | WS_VISIBLE,
            x, ADMIN1_Y, 45, LABEL_H, hWnd, NULL, NULL, NULL);
        SendMessageA(hLabelTitle, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 45;
        hEditTitle = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            x, ADMIN1_Y - 2, 110, CTRL_H, hWnd, (HMENU)ID_EDIT_TITLE, NULL, NULL);
        SendMessageA(hEditTitle, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 115;
        hLabelSec = CreateWindowA("STATIC", "Sec:", WS_CHILD | WS_VISIBLE,
            x, ADMIN1_Y, 35, LABEL_H, hWnd, NULL, NULL, NULL);
        SendMessageA(hLabelSec, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 35;
        hEditSection = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            x, ADMIN1_Y - 2, 40, CTRL_H, hWnd, (HMENU)ID_EDIT_SECTION, NULL, NULL);
        SendMessageA(hEditSection, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 45;
        hLabelInst = CreateWindowA("STATIC", "Inst:", WS_CHILD | WS_VISIBLE,
            x, ADMIN1_Y, 40, LABEL_H, hWnd, NULL, NULL, NULL);
        SendMessageA(hLabelInst, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 40;
        hEditInstructor = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            x, ADMIN1_Y - 2, 90, CTRL_H, hWnd, (HMENU)ID_EDIT_INSTRUCTOR, NULL, NULL);
        SendMessageA(hEditInstructor, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 95;
        hLabelTime = CreateWindowA("STATIC", "Time:", WS_CHILD | WS_VISIBLE,
            x, ADMIN1_Y, 40, LABEL_H, hWnd, NULL, NULL, NULL);
        SendMessageA(hLabelTime, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 40;
        hEditTime = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            x, ADMIN1_Y - 2, 90, CTRL_H, hWnd, (HMENU)ID_EDIT_TIME, NULL, NULL);
        SendMessageA(hEditTime, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // ===== Admin Panel Row 2: Add | Field(Combo) | New | Update | Delete =====
        x = m;
        hBtnAdd = CreateWindowA("BUTTON", "Add Course",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, ADMIN2_Y, 85, BTN_H, hWnd, (HMENU)ID_BTN_ADD, NULL, NULL);
        SendMessageA(hBtnAdd, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        x += 90;
        hLabelUpdateField = CreateWindowA("STATIC", "Field:", WS_CHILD | WS_VISIBLE,
            x, ADMIN2_Y + 5, 45, LABEL_H, hWnd, NULL, NULL, NULL);
        SendMessageA(hLabelUpdateField, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 45;
        hComboField = CreateWindowA("COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            x, ADMIN2_Y + 3, 115, 120, hWnd, (HMENU)ID_COMBO_FIELD, NULL, NULL);
        SendMessageA(hComboField, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"COURSE_TITLE");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"INSTRUCTOR");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"TIME");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"CLASSROOM");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"DAY");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"SEMESTER");
        SendMessageA(hComboField, CB_SETCURSEL, 0, 0);
        x += 120;
        hLabelNewVal = CreateWindowA("STATIC", "New:", WS_CHILD | WS_VISIBLE,
            x, ADMIN2_Y + 5, 40, LABEL_H, hWnd, NULL, NULL, NULL);
        SendMessageA(hLabelNewVal, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 40;
        hEditNewVal = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            x, ADMIN2_Y + 3, 130, CTRL_H, hWnd, (HMENU)ID_EDIT_NEWVAL, NULL, NULL);
        SendMessageA(hEditNewVal, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        x += 135;
        hBtnUpdate = CreateWindowA("BUTTON", "Update",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, ADMIN2_Y, 65, BTN_H, hWnd, (HMENU)ID_BTN_UPDATE, NULL, NULL);
        SendMessageA(hBtnUpdate, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        x += 70;
        hBtnDelete = CreateWindowA("BUTTON", "Delete",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, ADMIN2_Y, 65, BTN_H, hWnd, (HMENU)ID_BTN_DELETE, NULL, NULL);
        SendMessageA(hBtnDelete, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        // ===== Status Bar =====
        hStaticStatus = CreateWindowA("STATIC", "Ready. Connect to server to begin.",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            m, STATUS_Y, 780, 18, hWnd, (HMENU)ID_STATIC_STATUS, NULL, NULL);
        SendMessageA(hStaticStatus, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // ===== Initial disable =====
        EnableWindow(hBtnLogin, FALSE);
        EnableWindow(hBtnRegister, FALSE);
        EnableWindow(hBtnSearchCode, FALSE);
        EnableWindow(hBtnSearchInst, FALSE);
        EnableWindow(hBtnViewAll, FALSE);
        EnableWindow(hEditSearch, FALSE);
        EnableWindow(hEditUsername, FALSE);
        EnableWindow(hEditPassword, FALSE);
        EnableWindow(hBtnAdd, FALSE);
        EnableWindow(hBtnUpdate, FALSE);
        EnableWindow(hBtnDelete, FALSE);
        EnableWindow(hComboField, FALSE);
        EnableWindow(hEditNewVal, FALSE);
        EnableWindow(hEditCode, FALSE);
        EnableWindow(hEditTitle, FALSE);
        EnableWindow(hEditSection, FALSE);
        EnableWindow(hEditInstructor, FALSE);
        EnableWindow(hEditTime, FALSE);
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        // ==================== Connect ====================
        if (id == ID_BTN_CONNECT) {
            char buf[256] = {0};
            GetWindowTextA(hEditServer, buf, 255);
            std::string addr(buf);
            std::string ip = "127.0.0.1";
            int port = 54000;
            size_t colon = addr.find(':');
            if (colon != std::string::npos) {
                ip = addr.substr(0, colon);
                port = std::stoi(addr.substr(colon + 1));
            } else if (!addr.empty()) {
                ip = addr;
            }

            SetWindowTextA(hStaticStatus, "Connecting...");
            if (g_client.connect(ip, port)) {
                SetWindowTextA(hStaticStatus,
                    ("Connected to " + ip + ":" + std::to_string(port)).c_str());
                EnableWindow(hBtnConnect, FALSE);
                EnableWindow(hEditServer, FALSE);
                EnableWindow(hBtnLogin, TRUE);
                EnableWindow(hBtnRegister, TRUE);
                EnableWindow(hBtnSearchCode, TRUE);
                EnableWindow(hBtnSearchInst, TRUE);
                EnableWindow(hBtnViewAll, TRUE);
                EnableWindow(hEditSearch, TRUE);
                EnableWindow(hEditUsername, TRUE);
                EnableWindow(hEditPassword, TRUE);
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Connected to server ===");
                appendListBox(hListBox, "Enter username/password and click Login.");
            } else {
                SetWindowTextA(hStaticStatus, "Failed to connect! Is the server running?");
                appendListBox(hListBox, "ERROR: Connection failed - make sure server is running on " + ip + ":" + std::to_string(port));
            }
        }

        // ==================== Login ====================
        else if (id == ID_BTN_LOGIN) {
            char userBuf[128] = {0}, passBuf[128] = {0};
            GetWindowTextA(hEditUsername, userBuf, 127);
            GetWindowTextA(hEditPassword, passBuf, 127);
            std::string username(userBuf), password(passBuf);
            if (username.empty()) {
                SetWindowTextA(hStaticStatus, "Please enter username");
                SetFocus(hEditUsername);
                break;
            }
            std::string response = g_client.sendRequest(
                std::string(CMD_LOGIN) + " " + username + " " + password);
            if (response.find(SUCCESS_PREFIX) == 0) {
                g_loggedIn = true;
                g_isAdmin = (response.find("Role: Admin") != std::string::npos);
                g_username = username;
                SetWindowTextA(hStaticStatus,
                    ("Logged in as " + username +
                     (g_isAdmin ? " (Admin)" : " (Student)")).c_str());
                ShowWindow(hBtnLogin, SW_HIDE);
                ShowWindow(hBtnRegister, SW_HIDE);
                ShowWindow(hBtnLogout, SW_SHOW);
                EnableWindow(hEditUsername, FALSE);
                EnableWindow(hEditPassword, FALSE);
                if (g_isAdmin) {
                    EnableWindow(hBtnAdd, TRUE);
                    EnableWindow(hBtnUpdate, TRUE);
                    EnableWindow(hBtnDelete, TRUE);
                    EnableWindow(hComboField, TRUE);
                    EnableWindow(hEditNewVal, TRUE);
                    EnableWindow(hEditCode, TRUE);
                    EnableWindow(hEditTitle, TRUE);
                    EnableWindow(hEditSection, TRUE);
                    EnableWindow(hEditInstructor, TRUE);
                    EnableWindow(hEditTime, TRUE);
                }
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Login successful ===");
                appendListBox(hListBox, std::string("Welcome, ") + username +
                    (g_isAdmin ? " (Admin)!" : " (Student)!"));
                if (g_isAdmin)
                    appendListBox(hListBox, "Use the Admin Panel below to manage courses.");
                else
                    appendListBox(hListBox, "You have STUDENT access (query only).");

                // Auto-load all courses after login
                std::string resp = g_client.sendRequest(CMD_LIST_ALL);
                showQueryResult(hListBox, hStaticStatus, resp, "--- All Courses ---");
            } else {
                std::string clean = cleanResponse(response);
                SetWindowTextA(hStaticStatus, ("Login failed: " + clean).c_str());
                appendListBox(hListBox, "Login failed: " + clean);
            }
        }

        // ==================== Register ====================
        else if (id == ID_BTN_REGISTER) {
            char userBuf[128] = {0}, passBuf[128] = {0};
            GetWindowTextA(hEditUsername, userBuf, 127);
            GetWindowTextA(hEditPassword, passBuf, 127);
            std::string username(userBuf), password(passBuf);
            if (username.empty() || password.empty()) {
                SetWindowTextA(hStaticStatus, "Please enter username and password");
                break;
            }
            std::string response = g_client.sendRequest(
                std::string(CMD_REGISTER) + " " + username + " " + password);
            if (response.find(SUCCESS_PREFIX) == 0) {
                g_loggedIn = true;
                g_isAdmin = (response.find("Role: Admin") != std::string::npos);
                g_username = username;
                SetWindowTextA(hStaticStatus,
                    ("Registered & logged in as " + username +
                     (g_isAdmin ? " (Admin)" : " (Student)")).c_str());
                ShowWindow(hBtnLogin, SW_HIDE);
                ShowWindow(hBtnRegister, SW_HIDE);
                ShowWindow(hBtnLogout, SW_SHOW);
                EnableWindow(hEditUsername, FALSE);
                EnableWindow(hEditPassword, FALSE);
                if (g_isAdmin) {
                    EnableWindow(hBtnAdd, TRUE);
                    EnableWindow(hBtnUpdate, TRUE);
                    EnableWindow(hBtnDelete, TRUE);
                    EnableWindow(hComboField, TRUE);
                    EnableWindow(hEditNewVal, TRUE);
                    EnableWindow(hEditCode, TRUE);
                    EnableWindow(hEditTitle, TRUE);
                    EnableWindow(hEditSection, TRUE);
                    EnableWindow(hEditInstructor, TRUE);
                    EnableWindow(hEditTime, TRUE);
                }
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Registration successful ===");
                appendListBox(hListBox, std::string("Welcome, ") + username +
                    (g_isAdmin ? " (Admin)!" : " (Student)!"));
                std::string resp = g_client.sendRequest(CMD_LIST_ALL);
                showQueryResult(hListBox, hStaticStatus, resp, "--- All Courses ---");
            } else {
                std::string clean = cleanResponse(response);
                SetWindowTextA(hStaticStatus, ("Registration failed: " + clean).c_str());
                appendListBox(hListBox, "Registration failed: " + clean);
            }
        }

        // ==================== Logout ====================
        else if (id == ID_BTN_LOGOUT) {
            g_loggedIn = false;
            g_isAdmin = false;
            g_username = "";
            SetWindowTextA(hStaticStatus, "Logged out. You can login again.");
            ShowWindow(hBtnLogin, SW_SHOW);
            ShowWindow(hBtnRegister, SW_SHOW);
            ShowWindow(hBtnLogout, SW_HIDE);
            EnableWindow(hEditUsername, TRUE);
            EnableWindow(hEditPassword, TRUE);
            SetWindowTextA(hEditUsername, "");
            SetWindowTextA(hEditPassword, "");
            EnableWindow(hBtnAdd, FALSE);
            EnableWindow(hBtnUpdate, FALSE);
            EnableWindow(hBtnDelete, FALSE);
            EnableWindow(hComboField, FALSE);
            EnableWindow(hEditNewVal, FALSE);
            EnableWindow(hEditCode, FALSE);
            EnableWindow(hEditTitle, FALSE);
            EnableWindow(hEditSection, FALSE);
            EnableWindow(hEditInstructor, FALSE);
            EnableWindow(hEditTime, FALSE);
            clearListBox(hListBox);
            appendListBox(hListBox, "=== Logged out ===");
        }

        // ==================== Search by Code ====================
        else if (id == ID_BTN_SEARCH_CODE) {
            char buf[256] = {0};
            GetWindowTextA(hEditSearch, buf, 255);
            std::string code(buf);
            if (code.empty()) {
                SetWindowTextA(hStaticStatus, "Enter a course code");
                break;
            }
            std::string response = g_client.sendRequest(
                std::string(CMD_QUERY_CODE) + " " + code);
            showQueryResult(hListBox, hStaticStatus, response,
                "=== Search by Code: " + code + " ===");
        }

        // ==================== Search by Instructor ====================
        else if (id == ID_BTN_SEARCH_INST) {
            char buf[256] = {0};
            GetWindowTextA(hEditSearch, buf, 255);
            std::string name(buf);
            if (name.empty()) {
                SetWindowTextA(hStaticStatus, "Enter an instructor name");
                break;
            }
            std::string response = g_client.sendRequest(
                std::string(CMD_QUERY_INSTRUCTOR) + " " + name);
            showQueryResult(hListBox, hStaticStatus, response,
                "=== Search by Instructor: " + name + " ===");
        }

        // ==================== View All ====================
        else if (id == ID_BTN_VIEW_ALL) {
            std::string response = g_client.sendRequest(CMD_LIST_ALL);
            showQueryResult(hListBox, hStaticStatus, response,
                "=== All Courses ===");
        }

        // ==================== Add Course (Admin) ====================
        else if (id == ID_BTN_ADD) {
            if (!g_loggedIn || !g_isAdmin) {
                MessageBoxA(hWnd, "Admin privileges required!", "Error", MB_OK | MB_ICONERROR);
                break;
            }
            char code[128]={0}, title[128]={0}, section[128]={0};
            char instructor[128]={0}, time[128]={0};
            GetWindowTextA(hEditCode, code, 127);
            GetWindowTextA(hEditTitle, title, 127);
            GetWindowTextA(hEditSection, section, 127);
            GetWindowTextA(hEditInstructor, instructor, 127);
            GetWindowTextA(hEditTime, time, 127);
            if (strlen(code) == 0) {
                SetWindowTextA(hStaticStatus, "Course Code is required!");
                SetFocus(hEditCode);
                break;
            }
            // Build ADD command with pipe-separated fields
            std::string req = std::string(CMD_ADD) + " " +
                std::string(code) + "|" + std::string(title) + "|" +
                std::string(section) + "|" + std::string(instructor) + "|" +
                std::string(time);
            std::string response = g_client.sendRequest(req);
            std::string clean = cleanResponse(response);
            SetWindowTextA(hStaticStatus, clean.c_str());
            appendListBox(hListBox, "Add: " + clean);
            // Refresh list
            std::string resp = g_client.sendRequest(CMD_LIST_ALL);
            showQueryResult(hListBox, hStaticStatus, resp, "--- All Courses (refreshed) ---");
        }

        // ==================== Update Course (Admin) ====================
        else if (id == ID_BTN_UPDATE) {
            if (!g_loggedIn || !g_isAdmin) {
                MessageBoxA(hWnd, "Admin privileges required!", "Error", MB_OK | MB_ICONERROR);
                break;
            }
            char code[128]={0}, section[128]={0}, newVal[256]={0};
            GetWindowTextA(hEditCode, code, 127);
            GetWindowTextA(hEditSection, section, 127);
            GetWindowTextA(hEditNewVal, newVal, 255);
            if (strlen(code) == 0) {
                SetWindowTextA(hStaticStatus, "Enter course code to update");
                SetFocus(hEditCode);
                break;
            }
            if (strlen(newVal) == 0) {
                SetWindowTextA(hStaticStatus, "Enter new value");
                SetFocus(hEditNewVal);
                break;
            }
            char fieldBuf[64]={0};
            int sel = SendMessageA(hComboField, CB_GETCURSEL, 0, 0);
            if (sel == CB_ERR) sel = 0;
            SendMessageA(hComboField, CB_GETLBTEXT, sel, (LPARAM)fieldBuf);
            std::string field(fieldBuf);
            std::string req = std::string(CMD_UPDATE) + " " +
                std::string(code) + "|" + std::string(section) + "|" +
                field + "|" + std::string(newVal);
            std::string response = g_client.sendRequest(req);
            std::string clean = cleanResponse(response);
            SetWindowTextA(hStaticStatus, clean.c_str());
            appendListBox(hListBox, "Update: " + clean);
            // Refresh list
            std::string resp = g_client.sendRequest(CMD_LIST_ALL);
            showQueryResult(hListBox, hStaticStatus, resp, "--- All Courses (refreshed) ---");
        }

        // ==================== Delete Course (Admin) ====================
        else if (id == ID_BTN_DELETE) {
            if (!g_loggedIn || !g_isAdmin) {
                MessageBoxA(hWnd, "Admin privileges required!", "Error", MB_OK | MB_ICONERROR);
                break;
            }
            char code[128]={0}, section[128]={0};
            GetWindowTextA(hEditCode, code, 127);
            GetWindowTextA(hEditSection, section, 127);
            if (strlen(code) == 0) {
                SetWindowTextA(hStaticStatus, "Enter course code to delete");
                SetFocus(hEditCode);
                break;
            }
            std::string req = std::string(CMD_DELETE) + " " +
                std::string(code) + "|" + std::string(section);
            std::string response = g_client.sendRequest(req);
            std::string clean = cleanResponse(response);
            SetWindowTextA(hStaticStatus, clean.c_str());
            appendListBox(hListBox, "Delete: " + clean);
            // Refresh list
            std::string resp = g_client.sendRequest(CMD_LIST_ALL);
            showQueryResult(hListBox, hStaticStatus, resp, "--- All Courses (refreshed) ---");
        }

        break;
    }

    case WM_CLOSE: {
        if (g_client.isConnected()) {
            g_client.disconnect();
        }
        DestroyWindow(hWnd);
        break;
    }

    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }

    default:
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// ==================== WinMain ====================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBoxA(NULL, "WSAStartup failed!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class
    const char CLASS_NAME[] = "TimetableGUI";

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Window registration failed!", "Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        return 1;
    }

    // Center window on screen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int winWidth = 800;
    int winHeight = 440;
    int winX = (screenWidth - winWidth) / 2;
    int winY = (screenHeight - winHeight) / 2;

    // Create window
    HWND hWnd = CreateWindowExA(
        0, CLASS_NAME, "Course Timetable Inquiry System",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        winX, winY, winWidth, winHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) {
        MessageBoxA(NULL, "Window creation failed!", "Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Message loop
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    WSACleanup();
    return msg.wParam;
}
