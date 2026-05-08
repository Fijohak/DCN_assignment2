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
#define ID_BTN_REFRESH        1014
#define ID_BTN_SEARCH_CODE    1004
#define ID_BTN_SEARCH_INST    1005
#define ID_BTN_SEARCH_SEM     1015
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
#define ID_EDIT_CLASSROOM     2010
#define ID_EDIT_NEWVAL        2011
#define ID_EDIT_SEMESTER      2012
#define ID_EDIT_DAY           2013
#define ID_EDIT_START         2014
#define ID_EDIT_END           2015
#define ID_COMBO_FIELD        4001
#define ID_LIST_RESULT        3001
#define ID_STATIC_STATUS      5001

// ==================== Helpers ====================

// Strip response protocol wrappers for display.
std::string cleanResponse(const std::string& response) {
    std::string display = response;

    // Strip RESULT count=N\r\n header
    size_t pos = display.find(RESULT_START);
    if (pos == 0) {
        pos = display.find('\n');
        if (pos != std::string::npos)
            display = display.substr(pos + 1);
    }

    // Strip END\r\n footer
    pos = display.find(RESULT_END);
    if (pos != std::string::npos)
        display = display.substr(0, pos);

    // Strip response prefix
    if (display.find(OK_PREFIX) == 0)
        display = display.substr(strlen(OK_PREFIX));
    else if (display.find(SUCCESS_PREFIX) == 0)
        display = display.substr(strlen(SUCCESS_PREFIX));
    else if (display.find(FAILURE_PREFIX) == 0)
        display = display.substr(strlen(FAILURE_PREFIX));
    else if (display.find(ERROR_PREFIX) == 0)
        display = display.substr(strlen(ERROR_PREFIX));

    // Strip trailing \r
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

// Parse RESULT response and append each course line to the listbox.
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
    static HWND hEditCode, hEditTitle, hEditSection, hEditInstructor;
    static HWND hEditSemester, hEditDay, hEditStart, hEditEnd, hEditClassroom, hEditNewVal;
    static HWND hComboField;
    static HWND hBtnConnect, hBtnLogin, hBtnRegister, hBtnRefresh;
    static HWND hBtnSearchCode, hBtnSearchInst, hBtnSearchSem, hBtnViewAll;
    static HWND hBtnAdd, hBtnUpdate, hBtnDelete;
    static HWND hTitle;
    static int m = 15;

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

        // Title
        hTitle = CreateWindowA("STATIC", "Course Timetable Inquiry System",
            WS_CHILD | WS_VISIBLE | SS_CENTER, m, 10, 670, 30, hWnd, NULL, NULL, NULL);
        SendMessageA(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

        // Connection row
        CreateWindowA("STATIC", "Server:", WS_CHILD | WS_VISIBLE,
            m, 50, 50, 22, hWnd, NULL, NULL, NULL);
        hEditServer = CreateWindowA("EDIT", "127.0.0.1:8888",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            m + 55, 48, 200, 24, hWnd, (HMENU)ID_EDIT_SERVER, NULL, NULL);
        SendMessageA(hEditServer, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        hBtnConnect = CreateWindowA("BUTTON", "Connect",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 260, 48, 80, 24, hWnd, (HMENU)ID_BTN_CONNECT, NULL, NULL);
        SendMessageA(hBtnConnect, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        // Refresh button
        hBtnRefresh = CreateWindowA("BUTTON", "Refresh",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 350, 48, 70, 24, hWnd, (HMENU)ID_BTN_REFRESH, NULL, NULL);
        SendMessageA(hBtnRefresh, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // Login row (right side)
        CreateWindowA("STATIC", "User:", WS_CHILD | WS_VISIBLE,
            m + 460, 50, 35, 22, hWnd, NULL, NULL, NULL);
        hEditUsername = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            m + 495, 48, 90, 24, hWnd, (HMENU)ID_EDIT_USERNAME, NULL, NULL);
        SendMessageA(hEditUsername, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        CreateWindowA("STATIC", "Pwd:", WS_CHILD | WS_VISIBLE,
            m + 590, 50, 30, 22, hWnd, NULL, NULL, NULL);
        hEditPassword = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_LEFT,
            m + 620, 48, 80, 24, hWnd, (HMENU)ID_EDIT_PASSWORD, NULL, NULL);
        SendMessageA(hEditPassword, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        hBtnLogin = CreateWindowA("BUTTON", "Login",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 700, 48, 50, 24, hWnd, (HMENU)ID_BTN_LOGIN, NULL, NULL);
        SendMessageA(hBtnLogin, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        hBtnRegister = CreateWindowA("BUTTON", "Reg",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 755, 48, 35, 24, hWnd, (HMENU)ID_BTN_REGISTER, NULL, NULL);
        SendMessageA(hBtnRegister, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // Search row
        CreateWindowA("STATIC", "Search:", WS_CHILD | WS_VISIBLE,
            m, 82, 50, 22, hWnd, NULL, NULL, NULL);
        hEditSearch = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            m + 55, 80, 140, 24, hWnd, (HMENU)ID_EDIT_SEARCH, NULL, NULL);
        SendMessageA(hEditSearch, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        hBtnSearchCode = CreateWindowA("BUTTON", "By Code",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 200, 80, 80, 24, hWnd, (HMENU)ID_BTN_SEARCH_CODE, NULL, NULL);
        SendMessageA(hBtnSearchCode, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        hBtnSearchInst = CreateWindowA("BUTTON", "By Instructor",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 285, 80, 100, 24, hWnd, (HMENU)ID_BTN_SEARCH_INST, NULL, NULL);
        SendMessageA(hBtnSearchInst, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        hBtnSearchSem = CreateWindowA("BUTTON", "By Semester",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 390, 80, 100, 24, hWnd, (HMENU)ID_BTN_SEARCH_SEM, NULL, NULL);
        SendMessageA(hBtnSearchSem, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        hBtnViewAll = CreateWindowA("BUTTON", "View All",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 495, 80, 80, 24, hWnd, (HMENU)ID_BTN_VIEW_ALL, NULL, NULL);
        SendMessageA(hBtnViewAll, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // Results listbox
        hListBox = CreateWindowA("LISTBOX", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT,
            m, 112, 670, 200, hWnd, (HMENU)ID_LIST_RESULT, NULL, NULL);
        SendMessageA(hListBox, WM_SETFONT, (WPARAM)hFontMono, TRUE);

        // Admin panel - Row 1: semester | code | title | section | instructor
        CreateWindowA("STATIC", "Admin Panel:", WS_CHILD | WS_VISIBLE,
            m, 320, 100, 22, hWnd, NULL, NULL, NULL);

        CreateWindowA("STATIC", "Sem:", WS_CHILD | WS_VISIBLE,
            m + 115, 320, 30, 22, hWnd, NULL, NULL, NULL);
        hEditSemester = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 148, 318, 80, 24, hWnd, (HMENU)ID_EDIT_SEMESTER, NULL, NULL);
        SendMessageA(hEditSemester, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        CreateWindowA("STATIC", "Code:", WS_CHILD | WS_VISIBLE,
            m + 235, 320, 35, 22, hWnd, NULL, NULL, NULL);
        hEditCode = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 273, 318, 65, 24, hWnd, (HMENU)ID_EDIT_CODE, NULL, NULL);
        SendMessageA(hEditCode, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        CreateWindowA("STATIC", "Title:", WS_CHILD | WS_VISIBLE,
            m + 345, 320, 30, 22, hWnd, NULL, NULL, NULL);
        hEditTitle = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 378, 318, 75, 24, hWnd, (HMENU)ID_EDIT_TITLE, NULL, NULL);
        SendMessageA(hEditTitle, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        CreateWindowA("STATIC", "Sec:", WS_CHILD | WS_VISIBLE,
            m + 460, 320, 25, 22, hWnd, NULL, NULL, NULL);
        hEditSection = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 488, 318, 32, 24, hWnd, (HMENU)ID_EDIT_SECTION, NULL, NULL);
        SendMessageA(hEditSection, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        CreateWindowA("STATIC", "Inst:", WS_CHILD | WS_VISIBLE,
            m + 527, 320, 30, 22, hWnd, NULL, NULL, NULL);
        hEditInstructor = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 560, 318, 68, 24, hWnd, (HMENU)ID_EDIT_INSTRUCTOR, NULL, NULL);
        SendMessageA(hEditInstructor, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // Admin panel - Row 1b: day | start | end | classroom
        CreateWindowA("STATIC", "Day:", WS_CHILD | WS_VISIBLE,
            m + 115, 348, 30, 22, hWnd, NULL, NULL, NULL);
        hEditDay = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 148, 346, 40, 24, hWnd, (HMENU)ID_EDIT_DAY, NULL, NULL);
        SendMessageA(hEditDay, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        CreateWindowA("STATIC", "Start:", WS_CHILD | WS_VISIBLE,
            m + 195, 348, 35, 22, hWnd, NULL, NULL, NULL);
        hEditStart = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 233, 346, 50, 24, hWnd, (HMENU)ID_EDIT_START, NULL, NULL);
        SendMessageA(hEditStart, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        CreateWindowA("STATIC", "End:", WS_CHILD | WS_VISIBLE,
            m + 290, 348, 30, 22, hWnd, NULL, NULL, NULL);
        hEditEnd = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 323, 346, 50, 24, hWnd, (HMENU)ID_EDIT_END, NULL, NULL);
        SendMessageA(hEditEnd, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        CreateWindowA("STATIC", "Room:", WS_CHILD | WS_VISIBLE,
            m + 380, 348, 35, 22, hWnd, NULL, NULL, NULL);
        hEditClassroom = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 418, 346, 60, 24, hWnd, (HMENU)ID_EDIT_CLASSROOM, NULL, NULL);
        SendMessageA(hEditClassroom, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // Admin panel - Row 2: Add | Field | New | Update | Delete
        hBtnAdd = CreateWindowA("BUTTON", "Add Course",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 10, 380, 90, 28, hWnd, (HMENU)ID_BTN_ADD, NULL, NULL);
        SendMessageA(hBtnAdd, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        CreateWindowA("STATIC", "Field:", WS_CHILD | WS_VISIBLE,
            m + 108, 385, 35, 22, hWnd, NULL, NULL, NULL);
        hComboField = CreateWindowA("COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            m + 146, 383, 100, 100, hWnd, (HMENU)ID_COMBO_FIELD, NULL, NULL);
        SendMessageA(hComboField, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"COURSE_TITLE");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"INSTRUCTOR");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"TIME");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"CLASSROOM");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"DAY");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"SEMESTER");
        SendMessageA(hComboField, CB_SETCURSEL, 0, 0);

        CreateWindowA("STATIC", "New:", WS_CHILD | WS_VISIBLE,
            m + 255, 385, 30, 22, hWnd, NULL, NULL, NULL);
        hEditNewVal = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 288, 383, 120, 24, hWnd, (HMENU)ID_EDIT_NEWVAL, NULL, NULL);
        SendMessageA(hEditNewVal, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        hBtnUpdate = CreateWindowA("BUTTON", "Update",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 415, 383, 65, 28, hWnd, (HMENU)ID_BTN_UPDATE, NULL, NULL);
        SendMessageA(hBtnUpdate, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        hBtnDelete = CreateWindowA("BUTTON", "Delete",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 485, 383, 65, 28, hWnd, (HMENU)ID_BTN_DELETE, NULL, NULL);
        SendMessageA(hBtnDelete, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        // Status bar
        hStaticStatus = CreateWindowA("STATIC", "Ready. Connect to server to begin.",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            m, 420, 670, 18, hWnd, (HMENU)ID_STATIC_STATUS, NULL, NULL);
        SendMessageA(hStaticStatus, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // Initial disable of controls that require connection/login
        EnableWindow(hBtnLogin, FALSE);
        EnableWindow(hBtnRegister, FALSE);
        EnableWindow(hBtnRefresh, FALSE);
        EnableWindow(hBtnSearchCode, FALSE);
        EnableWindow(hBtnSearchInst, FALSE);
        EnableWindow(hBtnSearchSem, FALSE);
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
        EnableWindow(hEditSemester, FALSE);
        EnableWindow(hEditDay, FALSE);
        EnableWindow(hEditStart, FALSE);
        EnableWindow(hEditEnd, FALSE);
        EnableWindow(hEditClassroom, FALSE);
        break;
    }

    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);

        // Title
        MoveWindow(hTitle, m, 10, w - 2 * m, 30, TRUE);

        // Connection row
        int addrW = (w - 2 * m - 55 - 80 - 10 - 70 - 10 - 35 - 90 - 30 - 80 - 50) / 2;
        if (addrW < 80) addrW = 80;
        MoveWindow(hEditServer, m + 55, 48, addrW, 24, TRUE);
        MoveWindow(hBtnConnect, m + 55 + addrW + 10, 48, 80, 24, TRUE);
        MoveWindow(hBtnRefresh, m + 55 + addrW + 10 + 80 + 10, 48, 70, 24, TRUE);

        // Login row
        int refreshEnd = m + 55 + addrW + 10 + 80 + 10 + 70 + 10;
        int loginStart = w - m - 50 - 10 - 80 - 10 - 30 - 10 - 90 - 35;
        if (loginStart < refreshEnd) loginStart = refreshEnd + 20;
        MoveWindow(hEditUsername, loginStart + 35, 48, 90, 24, TRUE);
        MoveWindow(hEditPassword, loginStart + 35 + 90 + 10 + 30, 48, 80, 24, TRUE);
        MoveWindow(hBtnLogin, loginStart + 35 + 90 + 10 + 30 + 80 + 10, 48, 50, 24, TRUE);
        MoveWindow(hBtnRegister, loginStart + 35 + 90 + 10 + 30 + 80 + 10 + 55, 48, 35, 24, TRUE);

        // Search row
        int availForSearch = w - 2 * m - 55 - 80 - 100 - 100 - 80 - 50;
        if (availForSearch < 80) availForSearch = 80;
        MoveWindow(hEditSearch, m + 55, 80, availForSearch, 24, TRUE);

        int bx = m + 55 + availForSearch + 10;
        MoveWindow(hBtnSearchCode, bx, 80, 80, 24, TRUE);
        MoveWindow(hBtnSearchInst, bx + 90, 80, 100, 24, TRUE);
        MoveWindow(hBtnSearchSem, bx + 200, 80, 100, 24, TRUE);
        MoveWindow(hBtnViewAll, bx + 310, 80, 80, 24, TRUE);

        // Results list
        int listH = h - 112 - 130 - 18 - 15;
        if (listH < 50) listH = 50;
        MoveWindow(hListBox, m, 112, w - 2 * m, listH, TRUE);

        // Admin panel - Row 1
        int adminY = 112 + listH + 5;
        MoveWindow(hEditSemester, m + 148, adminY - 2, 80, 24, TRUE);
        MoveWindow(hEditCode, m + 273, adminY - 2, 65, 24, TRUE);
        MoveWindow(hEditTitle, m + 378, adminY - 2, 75, 24, TRUE);
        MoveWindow(hEditSection, m + 488, adminY - 2, 32, 24, TRUE);
        MoveWindow(hEditInstructor, m + 560, adminY - 2, 68, 24, TRUE);

        // Admin panel - Row 1b
        int adminYb = adminY + 28;
        MoveWindow(hEditDay, m + 148, adminYb - 2, 40, 24, TRUE);
        MoveWindow(hEditStart, m + 233, adminYb - 2, 50, 24, TRUE);
        MoveWindow(hEditEnd, m + 323, adminYb - 2, 50, 24, TRUE);
        MoveWindow(hEditClassroom, m + 418, adminYb - 2, 60, 24, TRUE);

        // Admin panel - Row 2
        int adminY2 = adminYb + 32;
        MoveWindow(hBtnAdd, m + 10, adminY2, 90, 28, TRUE);
        MoveWindow(hComboField, m + 146, adminY2 + 3, 100, 100, TRUE);
        MoveWindow(hEditNewVal, m + 288, adminY2 + 3, 120, 24, TRUE);
        MoveWindow(hBtnUpdate, m + 415, adminY2, 65, 28, TRUE);
        MoveWindow(hBtnDelete, m + 485, adminY2, 65, 28, TRUE);

        // Status bar
        MoveWindow(hStaticStatus, m, h - 18 - m, w - 2 * m, 18, TRUE);

        InvalidateRect(hWnd, NULL, TRUE);
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
            int port = 8888;
            size_t colon = addr.find(':');
            if (colon != std::string::npos) {
                ip = addr.substr(0, colon);
                port = std::stoi(addr.substr(colon + 1));
            } else if (!addr.empty()) {
                ip = addr;
            }

            if (g_client.connect(ip, port)) {
                std::string welcome = g_client.sendRequest("");
                SetWindowTextA(hStaticStatus,
                    ("Connected to " + ip + ":" + std::to_string(port)).c_str());
                EnableWindow(hBtnConnect, FALSE);
                EnableWindow(hEditServer, FALSE);
                EnableWindow(hBtnLogin, TRUE);
                EnableWindow(hBtnRegister, TRUE);
                EnableWindow(hBtnRefresh, TRUE);
                EnableWindow(hBtnSearchCode, TRUE);
                EnableWindow(hBtnSearchInst, TRUE);
                EnableWindow(hBtnSearchSem, TRUE);
                EnableWindow(hBtnViewAll, TRUE);
                EnableWindow(hEditSearch, TRUE);
                EnableWindow(hEditUsername, TRUE);
                EnableWindow(hEditPassword, TRUE);
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Connected to server ===");
                appendListBox(hListBox, cleanResponse(welcome));
                appendListBox(hListBox, "Enter admin credentials and click Login.");
            } else {
                SetWindowTextA(hStaticStatus, "Failed to connect!");
                appendListBox(hListBox, "ERROR: Connection failed");
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
                EnableWindow(hEditUsername, FALSE);
                EnableWindow(hEditPassword, FALSE);
                EnableWindow(hBtnLogin, FALSE);
                EnableWindow(hBtnRegister, FALSE);
                // Admin controls only for admin role
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
                    EnableWindow(hEditSemester, TRUE);
                    EnableWindow(hEditDay, TRUE);
                    EnableWindow(hEditStart, TRUE);
                    EnableWindow(hEditEnd, TRUE);
                    EnableWindow(hEditClassroom, TRUE);
                }
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Login successful ===");
                appendListBox(hListBox,
                    std::string("Welcome, ") + username +
                    (g_isAdmin ? " (Admin)!" : " (Student)!"));
                if (g_isAdmin)
                    appendListBox(hListBox, "Use the Admin Panel below to manage courses.");

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
                // Auto-login after successful registration
                g_loggedIn = true;
                g_isAdmin = (response.find("Role: Admin") != std::string::npos);
                g_username = username;
                SetWindowTextA(hStaticStatus,
                    ("Registered & logged in as " + username +
                     (g_isAdmin ? " (Admin)" : " (Student)")).c_str());
                EnableWindow(hEditUsername, FALSE);
                EnableWindow(hEditPassword, FALSE);
                EnableWindow(hBtnLogin, FALSE);
                EnableWindow(hBtnRegister, FALSE);
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
                    EnableWindow(hEditSemester, TRUE);
                    EnableWindow(hEditDay, TRUE);
                    EnableWindow(hEditStart, TRUE);
                    EnableWindow(hEditEnd, TRUE);
                    EnableWindow(hEditClassroom, TRUE);
                }
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Registration successful ===");
                appendListBox(hListBox,
                    std::string("Welcome, ") + username +
                    (g_isAdmin ? " (Admin)!" : " (Student)!"));
                // Auto-load all courses
                std::string resp = g_client.sendRequest(CMD_LIST_ALL);
                showQueryResult(hListBox, hStaticStatus, resp, "--- All Courses ---");
            } else {
                std::string clean = cleanResponse(response);
                SetWindowTextA(hStaticStatus, ("Registration failed: " + clean).c_str());
                appendListBox(hListBox, "Registration failed: " + clean);
            }
        }

        // ==================== Refresh ====================
        else if (id == ID_BTN_REFRESH) {
            std::string response = g_client.sendRequest(CMD_LIST_ALL);
            showQueryResult(hListBox, hStaticStatus, response, "=== All Courses (Refreshed) ===");
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

        // ==================== Search by Semester ====================
        else if (id == ID_BTN_SEARCH_SEM) {
            char buf[256] = {0};
            GetWindowTextA(hEditSearch, buf, 255);
            std::string sem(buf);
            if (sem.empty()) {
                SetWindowTextA(hStaticStatus, "Enter a semester (e.g. 2026 Spring)");
                break;
            }
            std::string response = g_client.sendRequest(
                std::string(CMD_QUERY_SEMESTER) + " " + sem);
            showQueryResult(hListBox, hStaticStatus, response,
                "=== Search by Semester: " + sem + " ===");
        }

        // ==================== View All ====================
        else if (id == ID_BTN_VIEW_ALL) {
            std::string response = g_client.sendRequest(CMD_LIST_ALL);
            showQueryResult(hListBox, hStaticStatus, response, "=== All Courses ===");
        }

        // ==================== Add Course ====================
        else if (id == ID_BTN_ADD) {
            if (!g_loggedIn) {
                MessageBoxA(hWnd, "Admin login required!", "Error", MB_OK | MB_ICONERROR);
                break;
            }
            char semester[128] = {0}, code[128] = {0}, title[128] = {0};
            char section[128] = {0}, instructor[128] = {0};
            char day[128] = {0}, start[128] = {0}, end[128] = {0}, classroom[128] = {0};
            GetWindowTextA(hEditSemester, semester, 127);
            GetWindowTextA(hEditCode, code, 127);
            GetWindowTextA(hEditTitle, title, 127);
            GetWindowTextA(hEditSection, section, 127);
            GetWindowTextA(hEditInstructor, instructor, 127);
            GetWindowTextA(hEditDay, day, 127);
            GetWindowTextA(hEditStart, start, 127);
            GetWindowTextA(hEditEnd, end, 127);
            GetWindowTextA(hEditClassroom, classroom, 127);

            if (strlen(code) == 0) {
                SetWindowTextA(hStaticStatus, "Course Code is required!");
                SetFocus(hEditCode);
                break;
            }

            // Build pipe-separated ADD command: ADD sem|code|title|sec|inst|day|start|end|room
            std::string req = std::string(CMD_ADD) + " " +
                std::string(semester) + "|" + std::string(code) + "|" +
                std::string(title) + "|" + std::string(section) + "|" +
                std::string(instructor) + "|" + std::string(day) + "|" +
                std::string(start) + "|" + std::string(end) + "|" +
                std::string(classroom);
            std::string response = g_client.sendRequest(req);
            std::string clean = cleanResponse(response);
            SetWindowTextA(hStaticStatus, clean.c_str());
            appendListBox(hListBox, "Add: " + clean);
        }

        // ==================== Update Course ====================
        else if (id == ID_BTN_UPDATE) {
            if (!g_loggedIn) {
                MessageBoxA(hWnd, "Admin login required!", "Error", MB_OK | MB_ICONERROR);
                break;
            }
            char code[128] = {0}, section[128] = {0}, newVal[256] = {0};
            GetWindowTextA(hEditCode, code, 127);
            GetWindowTextA(hEditSection, section, 127);
            GetWindowTextA(hEditNewVal, newVal, 255);
            if (strlen(code) == 0) {
                SetWindowTextA(hStaticStatus, "Enter course code to update");
                SetFocus(hEditCode);
                break;
            }
            if (strlen(section) == 0) {
                SetWindowTextA(hStaticStatus, "Enter section to update");
                SetFocus(hEditSection);
                break;
            }
            if (strlen(newVal) == 0) {
                SetWindowTextA(hStaticStatus, "Enter new value");
                SetFocus(hEditNewVal);
                break;
            }
            char fieldBuf[64] = {0};
            int sel = SendMessageA(hComboField, CB_GETCURSEL, 0, 0);
            SendMessageA(hComboField, CB_GETLBTEXT, sel, (LPARAM)fieldBuf);
            std::string field(fieldBuf);

            // Build pipe-separated UPDATE command: UPDATE code|section|field|newvalue
            std::string req = std::string(CMD_UPDATE) + " " +
                std::string(code) + "|" + std::string(section) + "|" +
                field + "|" + std::string(newVal);
            std::string response = g_client.sendRequest(req);
            std::string clean = cleanResponse(response);
            SetWindowTextA(hStaticStatus, clean.c_str());
            appendListBox(hListBox, "Update: " + clean);
        }

        // ==================== Delete Course ====================
        else if (id == ID_BTN_DELETE) {
            if (!g_loggedIn) {
                MessageBoxA(hWnd, "Admin login required!", "Error", MB_OK | MB_ICONERROR);
                break;
            }
            char code[128] = {0}, section[128] = {0};
            GetWindowTextA(hEditCode, code, 127);
            GetWindowTextA(hEditSection, section, 127);
            if (strlen(code) == 0) {
                SetWindowTextA(hStaticStatus, "Enter course code to delete");
                SetFocus(hEditCode);
                break;
            }
            if (strlen(section) == 0) {
                SetWindowTextA(hStaticStatus, "Enter section to delete");
                SetFocus(hEditSection);
                break;
            }

            std::string confirmMsg = std::string("Delete course ") + code +
                                     " section " + section + "?";
            int mbResult = MessageBoxA(hWnd, confirmMsg.c_str(), "Confirm Delete",
                                       MB_YESNO | MB_ICONQUESTION);
            if (mbResult != IDYES) break;

            // Build pipe-separated DELETE command: DELETE code|section
            std::string req = std::string(CMD_DELETE) + " " +
                std::string(code) + "|" + std::string(section);
            std::string response = g_client.sendRequest(req);
            std::string clean = cleanResponse(response);
            SetWindowTextA(hStaticStatus, clean.c_str());
            appendListBox(hListBox, "Delete: " + clean);
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
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

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
        return 1;
    }

    int winWidth = 800;
    int winHeight = 500;
    int winX = (GetSystemMetrics(SM_CXSCREEN) - winWidth) / 2;
    int winY = (GetSystemMetrics(SM_CYSCREEN) - winHeight) / 2;

    HWND hWnd = CreateWindowExA(
        0, CLASS_NAME, "Course Timetable Inquiry System",
        WS_OVERLAPPEDWINDOW,
        winX, winY, winWidth, winHeight,
        NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        MessageBoxA(NULL, "Window creation failed!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    g_client.disconnect();
    return msg.wParam;
}
