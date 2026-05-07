// ============================================
// Course Timetable Inquiry System - GUI Client
// ============================================
// Win32 API graphical client with responsive layout
// Build: g++ -std=c++11 -Wall client/gui_client.cpp client/network_client.cpp -o timetable_gui.exe -lws2_32 -lgdi32 -lcomctl32 -mwindows

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <cctype>
#include "../include/protocol.h"
#include "../include/network_client.h"

// ==================== Global State ====================
NetworkClient g_client;
bool g_loggedIn = false;
bool g_isAdmin = false;
std::string g_username;

// ==================== Client Cache ====================
struct ClientCacheEntry {
    std::string result;
    time_t timestamp;
};
std::map<std::string, ClientCacheEntry> g_clientCache;
const int CLIENT_CACHE_TTL = 15;

std::string getClientCached(const std::string& key) {
    auto it = g_clientCache.find(key);
    if (it != g_clientCache.end()) {
        time_t now = time(0);
        if (difftime(now, it->second.timestamp) < CLIENT_CACHE_TTL)
            return it->second.result;
        g_clientCache.erase(it);
    }
    return "";
}

void setClientCache(const std::string& key, const std::string& result) {
    ClientCacheEntry entry;
    entry.result = result;
    entry.timestamp = time(0);
    g_clientCache[key] = entry;
}

void clearClientCache() { g_clientCache.clear(); }

// Control IDs
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
#define ID_BTN_SEARCH_TIME    1011
#define ID_BTN_SEARCH_TITLE   1012
#define ID_BTN_SEARCH_ROOM    1013
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
#define ID_COMBO_FIELD        4001
#define ID_LIST_RESULT        3001
#define ID_STATIC_STATUS      5001

// ==================== Helpers ====================
std::string cleanResponse(const std::string& response) {
    std::string display = response;

    size_t pos = display.find(RESULT_START);
    if (pos != std::string::npos)
        display = display.substr(pos + strlen(RESULT_START));

    pos = display.find("\nEND\n");
    if (pos != std::string::npos)
        display = display.substr(0, pos);
    else {
        pos = display.find("\nEND");
        if (pos != std::string::npos)
            display = display.substr(0, pos);
    }

    if (display.find(SUCCESS_PREFIX) == 0)
        display = display.substr(strlen(SUCCESS_PREFIX));
    if (display.find(FAILURE_PREFIX) == 0)
        display = display.substr(strlen(FAILURE_PREFIX));
    if (display.find(ERROR_PREFIX) == 0)
        display = display.substr(strlen(ERROR_PREFIX));

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
        if (!line.empty()) appendListBox(hLB, line);
    }
}

// ==================== Window Procedure ====================
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hListBox, hStaticStatus;
    static HWND hEditServer, hEditUsername, hEditPassword, hEditSearch;
    static HWND hEditCode, hEditTitle, hEditSection, hEditInstructor, hEditTime, hEditClassroom, hEditNewVal;
    static HWND hComboField;
    static HWND hBtnConnect, hBtnLogin, hBtnRegister, hBtnLogout;
    static HWND hBtnSearchCode, hBtnSearchInst, hBtnViewAll;
    static HWND hBtnSearchTime, hBtnSearchTitle, hBtnSearchRoom;
    static HWND hBtnAdd, hBtnUpdate, hBtnDelete;
    static HWND hTitle;
    static HWND hLabelCode, hLabelTitle, hLabelSec, hLabelInst, hLabelTime, hLabelRoom;
    static HWND hLabelAdminPanel, hLabelUpdateField, hLabelNewVal;
    static HWND hLabelUser, hLabelPwd;
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
            m + 200, 48, 80, 24, hWnd, (HMENU)ID_BTN_CONNECT, NULL, NULL);
        SendMessageA(hBtnConnect, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        // Login row
        hLabelUser = CreateWindowA("STATIC", "User:", WS_CHILD | WS_VISIBLE,
            m + 290, 50, 35, 22, hWnd, NULL, NULL, NULL);
        hEditUsername = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            m + 325, 48, 100, 24, hWnd, (HMENU)ID_EDIT_USERNAME, NULL, NULL);
        SendMessageA(hEditUsername, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        hLabelPwd = CreateWindowA("STATIC", "Pwd:", WS_CHILD | WS_VISIBLE,
            m + 430, 50, 30, 22, hWnd, NULL, NULL, NULL);
        hEditPassword = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD | ES_LEFT,
            m + 460, 48, 100, 24, hWnd, (HMENU)ID_EDIT_PASSWORD, NULL, NULL);
        SendMessageA(hEditPassword, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        hBtnLogin = CreateWindowA("BUTTON", "Login",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 565, 48, 60, 24, hWnd, (HMENU)ID_BTN_LOGIN, NULL, NULL);
        SendMessageA(hBtnLogin, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        hBtnRegister = CreateWindowA("BUTTON", "Reg",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 630, 48, 45, 24, hWnd, (HMENU)ID_BTN_REGISTER, NULL, NULL);
        SendMessageA(hBtnRegister, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        hBtnLogout = CreateWindowA("BUTTON", "Logout",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 565, 48, 80, 24, hWnd, (HMENU)ID_BTN_LOGOUT, NULL, NULL);
        SendMessageA(hBtnLogout, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        ShowWindow(hBtnLogout, SW_HIDE);

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
        hBtnViewAll = CreateWindowA("BUTTON", "View All",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 390, 80, 80, 24, hWnd, (HMENU)ID_BTN_VIEW_ALL, NULL, NULL);
        SendMessageA(hBtnViewAll, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // Advanced search buttons
        hBtnSearchTime = CreateWindowA("BUTTON", "By Time",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 475, 80, 70, 24, hWnd, (HMENU)ID_BTN_SEARCH_TIME, NULL, NULL);
        SendMessageA(hBtnSearchTime, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        hBtnSearchTitle = CreateWindowA("BUTTON", "By Title",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 550, 80, 70, 24, hWnd, (HMENU)ID_BTN_SEARCH_TITLE, NULL, NULL);
        SendMessageA(hBtnSearchTitle, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        hBtnSearchRoom = CreateWindowA("BUTTON", "By Room",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 625, 80, 70, 24, hWnd, (HMENU)ID_BTN_SEARCH_ROOM, NULL, NULL);
        SendMessageA(hBtnSearchRoom, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // Results listbox
        hListBox = CreateWindowA("LISTBOX", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT,
            m, 112, 670, 200, hWnd, (HMENU)ID_LIST_RESULT, NULL, NULL);
        SendMessageA(hListBox, WM_SETFONT, (WPARAM)hFontMono, TRUE);

        // Admin Panel - Row 1: Code | Title | Sec | Instructor | Time | Room
        hLabelAdminPanel = CreateWindowA("STATIC", "Admin Panel:",
            WS_CHILD | WS_VISIBLE, m, 320, 100, 22, hWnd, NULL, NULL, NULL);

        hLabelCode = CreateWindowA("STATIC", "Code:", WS_CHILD | WS_VISIBLE,
            m + 115, 320, 35, 22, hWnd, NULL, NULL, NULL);
        hEditCode = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 153, 318, 65, 24, hWnd, (HMENU)ID_EDIT_CODE, NULL, NULL);
        SendMessageA(hEditCode, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        hLabelTitle = CreateWindowA("STATIC", "Title:", WS_CHILD | WS_VISIBLE,
            m + 225, 320, 30, 22, hWnd, NULL, NULL, NULL);
        hEditTitle = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 258, 318, 75, 24, hWnd, (HMENU)ID_EDIT_TITLE, NULL, NULL);
        SendMessageA(hEditTitle, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        hLabelSec = CreateWindowA("STATIC", "Sec:", WS_CHILD | WS_VISIBLE,
            m + 340, 320, 25, 22, hWnd, NULL, NULL, NULL);
        hEditSection = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 368, 318, 32, 24, hWnd, (HMENU)ID_EDIT_SECTION, NULL, NULL);
        SendMessageA(hEditSection, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        hLabelInst = CreateWindowA("STATIC", "Inst:", WS_CHILD | WS_VISIBLE,
            m + 407, 320, 30, 22, hWnd, NULL, NULL, NULL);
        hEditInstructor = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 440, 318, 68, 24, hWnd, (HMENU)ID_EDIT_INSTRUCTOR, NULL, NULL);
        SendMessageA(hEditInstructor, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        hLabelTime = CreateWindowA("STATIC", "Time:", WS_CHILD | WS_VISIBLE,
            m + 515, 320, 30, 22, hWnd, NULL, NULL, NULL);
        hEditTime = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 548, 318, 68, 24, hWnd, (HMENU)ID_EDIT_TIME, NULL, NULL);
        SendMessageA(hEditTime, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        hLabelRoom = CreateWindowA("STATIC", "Room:", WS_CHILD | WS_VISIBLE,
            m + 623, 320, 35, 22, hWnd, NULL, NULL, NULL);
        hEditClassroom = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 661, 318, 52, 24, hWnd, (HMENU)ID_EDIT_CLASSROOM, NULL, NULL);
        SendMessageA(hEditClassroom, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // Admin Panel - Row 2: Add | Field | New | Update | Delete
        hBtnAdd = CreateWindowA("BUTTON", "Add Course",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 10, 352, 90, 28, hWnd, (HMENU)ID_BTN_ADD, NULL, NULL);
        SendMessageA(hBtnAdd, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        hLabelUpdateField = CreateWindowA("STATIC", "Field:", WS_CHILD | WS_VISIBLE,
            m + 108, 357, 35, 22, hWnd, NULL, NULL, NULL);
        hComboField = CreateWindowA("COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            m + 146, 355, 85, 100, hWnd, (HMENU)ID_COMBO_FIELD, NULL, NULL);
        SendMessageA(hComboField, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"title");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"instructor");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"time");
        SendMessageA(hComboField, CB_ADDSTRING, 0, (LPARAM)"classroom");
        SendMessageA(hComboField, CB_SETCURSEL, 0, 0);

        hLabelNewVal = CreateWindowA("STATIC", "New:", WS_CHILD | WS_VISIBLE,
            m + 238, 357, 30, 22, hWnd, NULL, NULL, NULL);
        hEditNewVal = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            m + 271, 355, 100, 24, hWnd, (HMENU)ID_EDIT_NEWVAL, NULL, NULL);
        SendMessageA(hEditNewVal, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        hBtnUpdate = CreateWindowA("BUTTON", "Update",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 378, 355, 65, 28, hWnd, (HMENU)ID_BTN_UPDATE, NULL, NULL);
        SendMessageA(hBtnUpdate, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        hBtnDelete = CreateWindowA("BUTTON", "Delete",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            m + 448, 355, 65, 28, hWnd, (HMENU)ID_BTN_DELETE, NULL, NULL);
        SendMessageA(hBtnDelete, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        // Status bar
        hStaticStatus = CreateWindowA("STATIC", "Ready. Connect to server to begin.",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            m, 395, 670, 18, hWnd, (HMENU)ID_STATIC_STATUS, NULL, NULL);
        SendMessageA(hStaticStatus, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        // Initial disable of auth-dependent controls
        EnableWindow(hBtnLogin, FALSE);
        EnableWindow(hBtnRegister, FALSE);
        EnableWindow(hBtnSearchCode, FALSE);
        EnableWindow(hBtnSearchInst, FALSE);
        EnableWindow(hBtnViewAll, FALSE);
        EnableWindow(hBtnSearchTime, FALSE);
        EnableWindow(hBtnSearchTitle, FALSE);
        EnableWindow(hBtnSearchRoom, FALSE);
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
        EnableWindow(hEditClassroom, FALSE);
        break;
    }

    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);

        // Title
        MoveWindow(hTitle, m, 10, w - 2 * m, 30, TRUE);

        // Connection / login row
        int addrW = (w - 2 * m - 55 - 80 - 35 - 100 - 30 - 100 - 60 - 45 - 80) / 2;
        if (addrW < 80) addrW = 80;
        MoveWindow(hEditServer, m + 55, 48, addrW, 24, TRUE);
        MoveWindow(hBtnConnect, m + 55 + addrW + 10, 48, 80, 24, TRUE);

        int rightX = w - m;
        MoveWindow(hBtnRegister, rightX - 45, 48, 45, 24, TRUE);
        MoveWindow(hBtnLogin, rightX - 45 - 10 - 60, 48, 60, 24, TRUE);
        MoveWindow(hBtnLogout, rightX - 80, 48, 80, 24, TRUE);
        MoveWindow(hEditPassword, rightX - 45 - 10 - 60 - 10 - 100, 48, 100, 24, TRUE);
        MoveWindow(hLabelPwd, rightX - 45 - 10 - 60 - 10 - 100 - 10 - 30, 50, 30, 22, TRUE);
        MoveWindow(hEditUsername, rightX - 45 - 10 - 60 - 10 - 100 - 10 - 30 - 10 - 100, 48, 100, 24, TRUE);
        MoveWindow(hLabelUser, rightX - 45 - 10 - 60 - 10 - 100 - 10 - 30 - 10 - 100 - 10 - 35, 50, 35, 22, TRUE);

        // Search row
        int availForSearch = w - 2 * m - 55 - 80 - 100 - 80 - 70 - 70 - 70 - 60;
        if (availForSearch < 80) availForSearch = 80;
        MoveWindow(hEditSearch, m + 55, 80, availForSearch, 24, TRUE);

        int bx = m + 55 + availForSearch + 10;
        MoveWindow(hBtnSearchCode, bx, 80, 80, 24, TRUE);
        MoveWindow(hBtnSearchInst, bx + 90, 80, 100, 24, TRUE);
        MoveWindow(hBtnViewAll, bx + 200, 80, 80, 24, TRUE);
        MoveWindow(hBtnSearchTime, bx + 290, 80, 70, 24, TRUE);
        MoveWindow(hBtnSearchTitle, bx + 370, 80, 70, 24, TRUE);
        MoveWindow(hBtnSearchRoom, bx + 450, 80, 70, 24, TRUE);

        // Results list
        int listH = h - 112 - 115 - 18 - 15 - 15;
        if (listH < 50) listH = 50;
        MoveWindow(hListBox, m, 112, w - 2 * m, listH, TRUE);

        // Admin Panel - Row 1
        int adminY = 112 + listH + 5;
        MoveWindow(hLabelAdminPanel, m, adminY, 100, 22, TRUE);

        // Dynamic widths for admin fields
        int totalW = w - 2 * m - 115; // space after "Admin Panel:"
        int roomW = 52;
        int timeW = 68;
        int instW = 68;
        int secW = 32;
        int codeW = 65;
        int titleW = totalW - codeW - 35 - secW - 25 - instW - 30 - timeW - 30 - roomW - 35 - 80;
        if (titleW < 50) titleW = 50;

        int x = m + 115;
        MoveWindow(hLabelCode, x, adminY, 35, 22, TRUE);
        MoveWindow(hEditCode, x + 38, adminY - 2, codeW, 24, TRUE);
        x += 38 + codeW + 8;

        MoveWindow(hLabelTitle, x, adminY, 30, 22, TRUE);
        MoveWindow(hEditTitle, x + 33, adminY - 2, titleW, 24, TRUE);
        x += 33 + titleW + 8;

        MoveWindow(hLabelSec, x, adminY, 25, 22, TRUE);
        MoveWindow(hEditSection, x + 28, adminY - 2, secW, 24, TRUE);
        x += 28 + secW + 8;

        MoveWindow(hLabelInst, x, adminY, 30, 22, TRUE);
        MoveWindow(hEditInstructor, x + 33, adminY - 2, instW, 24, TRUE);
        x += 33 + instW + 8;

        MoveWindow(hLabelTime, x, adminY, 30, 22, TRUE);
        MoveWindow(hEditTime, x + 33, adminY - 2, timeW, 24, TRUE);
        x += 33 + timeW + 8;

        MoveWindow(hLabelRoom, x, adminY, 35, 22, TRUE);
        MoveWindow(hEditClassroom, x + 38, adminY - 2, roomW, 24, TRUE);

        // Admin Panel - Row 2
        int adminY2 = adminY + 32;
        int newValW = w - 2 * m - 10 - 90 - 35 - 85 - 30 - 65 - 65 - 60;
        if (newValW < 80) newValW = 80;
        MoveWindow(hBtnAdd, m + 10, adminY2, 90, 28, TRUE);
        MoveWindow(hLabelUpdateField, m + 108, adminY2 + 5, 35, 22, TRUE);
        MoveWindow(hComboField, m + 146, adminY2 + 3, 85, 100, TRUE);
        MoveWindow(hLabelNewVal, m + 238, adminY2 + 5, 30, 22, TRUE);
        MoveWindow(hEditNewVal, m + 271, adminY2 + 3, newValW, 24, TRUE);
        MoveWindow(hBtnUpdate, w - m - 65 - 65 - 10, adminY2, 65, 28, TRUE);
        MoveWindow(hBtnDelete, w - m - 65, adminY2, 65, 28, TRUE);

        // Status bar
        MoveWindow(hStaticStatus, m, h - 18 - m, w - 2 * m, 18, TRUE);

        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);

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
                EnableWindow(hBtnSearchCode, TRUE);
                EnableWindow(hBtnSearchInst, TRUE);
                EnableWindow(hBtnViewAll, TRUE);
                EnableWindow(hBtnSearchTime, TRUE);
                EnableWindow(hBtnSearchTitle, TRUE);
                EnableWindow(hBtnSearchRoom, TRUE);
                EnableWindow(hEditSearch, TRUE);
                EnableWindow(hEditUsername, TRUE);
                EnableWindow(hEditPassword, TRUE);
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Connected to server ===");
                appendListBoxText(hListBox, cleanResponse(welcome));
                appendListBox(hListBox, "Enter username/password and click Login.");
            } else {
                SetWindowTextA(hStaticStatus, "Failed to connect!");
                appendListBox(hListBox, "ERROR: Connection failed");
            }
        }
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
                g_username = username;
                g_isAdmin = (response.find("Admin") != std::string::npos);
                SetWindowTextA(hStaticStatus,
                    ("Logged in as " + username + " (" +
                     (g_isAdmin ? "Admin" : "Student") + ")").c_str());
                ShowWindow(hBtnLogin, SW_HIDE);
                ShowWindow(hBtnRegister, SW_HIDE);
                ShowWindow(hBtnLogout, SW_SHOW);
                EnableWindow(hEditUsername, FALSE);
                EnableWindow(hEditPassword, FALSE);
                clearClientCache();
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
                    EnableWindow(hEditClassroom, TRUE);
                }
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Login successful ===");
                appendListBox(hListBox, "Welcome, " + username + "!");
                if (g_isAdmin) {
                    appendListBox(hListBox, "You have ADMIN privileges.");
                    appendListBox(hListBox, "Use the Admin Panel below to manage courses.");
                } else {
                    appendListBox(hListBox, "You have STUDENT access (query only).");
                }
            } else {
                std::string clean = cleanResponse(response);
                SetWindowTextA(hStaticStatus, ("Login failed: " + clean).c_str());
                appendListBox(hListBox, "Login failed: " + clean);
            }
        }
        else if (id == ID_BTN_REGISTER) {
            char userBuf[128] = {0}, passBuf[128] = {0};
            GetWindowTextA(hEditUsername, userBuf, 127);
            GetWindowTextA(hEditPassword, passBuf, 127);
            std::string username(userBuf), password(passBuf);
            if (username.empty() || password.empty()) {
                SetWindowTextA(hStaticStatus, "Please enter both username and password");
                break;
            }
            std::string response = g_client.sendRequest(
                std::string(CMD_REGISTER) + " " + username + " " + password);
            std::string clean = cleanResponse(response);
            SetWindowTextA(hStaticStatus, clean.c_str());
            appendListBox(hListBox, "Register: " + clean);
        }
        else if (id == ID_BTN_LOGOUT) {
            g_client.sendRequest(std::string(CMD_LOGOUT));
            g_loggedIn = false;
            g_isAdmin = false;
            g_username = "";
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
            EnableWindow(hEditClassroom, FALSE);
            clearClientCache();
            clearListBox(hListBox);
            appendListBox(hListBox, "=== Logged out ===");
            SetWindowTextA(hStaticStatus, "Logged out. You can login again.");
        }
        else if (id == ID_BTN_SEARCH_CODE) {
            char buf[256] = {0};
            GetWindowTextA(hEditSearch, buf, 255);
            std::string code(buf);
            if (code.empty()) {
                SetWindowTextA(hStaticStatus, "Enter a course code in the keyword box");
                break;
            }
            std::string cacheKey = std::string(CMD_QUERY) + " " + QUERY_CODE + " " + code;
            std::string cached = getClientCached(cacheKey);
            if (!cached.empty()) {
                SetWindowTextA(hStaticStatus, ("[Cached] Results for: " + code).c_str());
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Search by Code: " + code + " ===");
                appendListBoxText(hListBox, cached);
            } else {
                std::string response = g_client.sendRequest(cacheKey);
                if (response.find(ERROR_PREFIX) == 0 || response.find(FAILURE_PREFIX) == 0) {
                    SetWindowTextA(hStaticStatus, ("Error: " + cleanResponse(response)).c_str());
                    appendListBox(hListBox, "Error: " + cleanResponse(response));
                } else {
                    std::string clean = cleanResponse(response);
                    setClientCache(cacheKey, clean);
                    clearListBox(hListBox);
                    appendListBox(hListBox, "=== Search by Code: " + code + " ===");
                    appendListBoxText(hListBox, clean);
                    SetWindowTextA(hStaticStatus, ("Results for: " + code).c_str());
                }
            }
        }
        else if (id == ID_BTN_SEARCH_INST) {
            char buf[256] = {0};
            GetWindowTextA(hEditSearch, buf, 255);
            std::string name(buf);
            if (name.empty()) {
                SetWindowTextA(hStaticStatus, "Enter an instructor name");
                break;
            }
            std::string cacheKey = std::string(CMD_QUERY) + " " + QUERY_INSTRUCTOR + " " + name;
            std::string cached = getClientCached(cacheKey);
            if (!cached.empty()) {
                SetWindowTextA(hStaticStatus, ("[Cached] Results for: " + name).c_str());
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Search by Instructor: " + name + " ===");
                appendListBoxText(hListBox, cached);
            } else {
                std::string response = g_client.sendRequest(cacheKey);
                if (response.find(ERROR_PREFIX) == 0 || response.find(FAILURE_PREFIX) == 0) {
                    SetWindowTextA(hStaticStatus, ("Error: " + cleanResponse(response)).c_str());
                    appendListBox(hListBox, "Error: " + cleanResponse(response));
                } else {
                    std::string clean = cleanResponse(response);
                    setClientCache(cacheKey, clean);
                    clearListBox(hListBox);
                    appendListBox(hListBox, "=== Search by Instructor: " + name + " ===");
                    appendListBoxText(hListBox, clean);
                    SetWindowTextA(hStaticStatus, ("Results for: " + name).c_str());
                }
            }
        }
        else if (id == ID_BTN_VIEW_ALL) {
            std::string response = g_client.sendRequest(
                std::string(CMD_QUERY) + " " + QUERY_ALL);
            if (response.find(ERROR_PREFIX) == 0 || response.find(FAILURE_PREFIX) == 0) {
                SetWindowTextA(hStaticStatus, ("Error: " + cleanResponse(response)).c_str());
                appendListBox(hListBox, "Error: " + cleanResponse(response));
            } else {
                std::string clean = cleanResponse(response);
                clearListBox(hListBox);
                appendListBox(hListBox, "=== All Courses ===");
                appendListBoxText(hListBox, clean);
                SetWindowTextA(hStaticStatus, "Showing all courses");
            }
        }
        else if (id == ID_BTN_SEARCH_TIME) {
            char buf[256] = {0};
            GetWindowTextA(hEditSearch, buf, 255);
            std::string keyword(buf);
            if (keyword.empty()) {
                SetWindowTextA(hStaticStatus, "Enter a time/day (e.g., Mon, 10:00)");
                break;
            }
            std::string response = g_client.sendRequest(
                std::string(CMD_QUERY) + " " + QUERY_TIME + " " + keyword);
            if (response.find(ERROR_PREFIX) == 0 || response.find(FAILURE_PREFIX) == 0) {
                SetWindowTextA(hStaticStatus, ("Error: " + cleanResponse(response)).c_str());
                appendListBox(hListBox, "Error: " + cleanResponse(response));
            } else {
                std::string clean = cleanResponse(response);
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Search by Time: " + keyword + " ===");
                appendListBoxText(hListBox, clean);
                SetWindowTextA(hStaticStatus, ("Time search: " + keyword).c_str());
            }
        }
        else if (id == ID_BTN_SEARCH_TITLE) {
            char buf[256] = {0};
            GetWindowTextA(hEditSearch, buf, 255);
            std::string keyword(buf);
            if (keyword.empty()) {
                SetWindowTextA(hStaticStatus, "Enter a title keyword");
                break;
            }
            std::string response = g_client.sendRequest(
                std::string(CMD_QUERY) + " " + QUERY_TITLE + " " + keyword);
            if (response.find(ERROR_PREFIX) == 0 || response.find(FAILURE_PREFIX) == 0) {
                SetWindowTextA(hStaticStatus, ("Error: " + cleanResponse(response)).c_str());
                appendListBox(hListBox, "Error: " + cleanResponse(response));
            } else {
                std::string clean = cleanResponse(response);
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Search by Title: " + keyword + " ===");
                appendListBoxText(hListBox, clean);
                SetWindowTextA(hStaticStatus, ("Title search: " + keyword).c_str());
            }
        }
        else if (id == ID_BTN_SEARCH_ROOM) {
            char buf[256] = {0};
            GetWindowTextA(hEditSearch, buf, 255);
            std::string keyword(buf);
            if (keyword.empty()) {
                SetWindowTextA(hStaticStatus, "Enter a classroom keyword");
                break;
            }
            std::string response = g_client.sendRequest(
                std::string(CMD_QUERY) + " " + QUERY_CLASSROOM + " " + keyword);
            if (response.find(ERROR_PREFIX) == 0 || response.find(FAILURE_PREFIX) == 0) {
                SetWindowTextA(hStaticStatus, ("Error: " + cleanResponse(response)).c_str());
                appendListBox(hListBox, "Error: " + cleanResponse(response));
            } else {
                std::string clean = cleanResponse(response);
                clearListBox(hListBox);
                appendListBox(hListBox, "=== Search by Classroom: " + keyword + " ===");
                appendListBoxText(hListBox, clean);
                SetWindowTextA(hStaticStatus, ("Room search: " + keyword).c_str());
            }
        }
        else if (id == ID_BTN_ADD) {
            if (!g_loggedIn || !g_isAdmin) {
                MessageBoxA(hWnd, "Admin privileges required!", "Error", MB_OK | MB_ICONERROR);
                break;
            }
            char code[128] = {0}, title[128] = {0}, section[128] = {0};
            char instructor[128] = {0}, time[128] = {0}, classroom[128] = {0};
            GetWindowTextA(hEditCode, code, 127);
            GetWindowTextA(hEditTitle, title, 127);
            GetWindowTextA(hEditSection, section, 127);
            GetWindowTextA(hEditInstructor, instructor, 127);
            GetWindowTextA(hEditTime, time, 127);
            GetWindowTextA(hEditClassroom, classroom, 127);
            if (strlen(code) == 0) {
                SetWindowTextA(hStaticStatus, "Course Code is required!");
                SetFocus(hEditCode);
                break;
            }
            std::string req = std::string(CMD_ADD) + " " + std::string(code) + " " +
                              std::string(title) + " " + std::string(section) + " " +
                              std::string(instructor) + " " + std::string(time) + " " +
                              std::string(classroom);
            std::string response = g_client.sendRequest(req);
            if (response.find(SUCCESS_PREFIX) == 0) {
                clearClientCache();
                std::string clean = cleanResponse(response);
                SetWindowTextA(hStaticStatus, clean.c_str());
                appendListBox(hListBox, "Add: " + clean);
            } else {
                SetWindowTextA(hStaticStatus, ("Error: " + cleanResponse(response)).c_str());
                appendListBox(hListBox, "Error: " + cleanResponse(response));
            }
        }
        else if (id == ID_BTN_UPDATE) {
            if (!g_loggedIn || !g_isAdmin) {
                MessageBoxA(hWnd, "Admin privileges required!", "Error", MB_OK | MB_ICONERROR);
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
            std::string req = std::string(CMD_UPDATE) + " " + std::string(code) + " " +
                              std::string(section) + " " + field + " " + std::string(newVal);
            std::string response = g_client.sendRequest(req);
            if (response.find(SUCCESS_PREFIX) == 0) {
                clearClientCache();
                std::string clean = cleanResponse(response);
                SetWindowTextA(hStaticStatus, clean.c_str());
                appendListBox(hListBox, "Update: " + clean);
            } else {
                SetWindowTextA(hStaticStatus, ("Error: " + cleanResponse(response)).c_str());
                appendListBox(hListBox, "Error: " + cleanResponse(response));
            }
        }
        else if (id == ID_BTN_DELETE) {
            if (!g_loggedIn || !g_isAdmin) {
                MessageBoxA(hWnd, "Admin privileges required!", "Error", MB_OK | MB_ICONERROR);
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
            // Confirmation dialog
            std::string confirmMsg = std::string("Delete course ") + code +
                                     " section " + section + "?";
            int mbResult = MessageBoxA(hWnd, confirmMsg.c_str(), "Confirm Delete",
                                       MB_YESNO | MB_ICONQUESTION);
            if (mbResult != IDYES) break;

            std::string req = std::string(CMD_DELETE) + " " + std::string(code) +
                              " " + std::string(section);
            std::string response = g_client.sendRequest(req);
            if (response.find(SUCCESS_PREFIX) == 0) {
                clearClientCache();
                std::string clean = cleanResponse(response);
                SetWindowTextA(hStaticStatus, clean.c_str());
                appendListBox(hListBox, "Delete: " + clean);
            } else {
                SetWindowTextA(hStaticStatus, ("Error: " + cleanResponse(response)).c_str());
                appendListBox(hListBox, "Error: " + cleanResponse(response));
            }
        }
        break;
    }

    case WM_CLOSE: {
        if (g_client.isConnected()) {
            g_client.sendRequest(std::string(CMD_LOGOUT));
        }
        g_client.disconnect();
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

    int winWidth = 750;
    int winHeight = 480;
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
