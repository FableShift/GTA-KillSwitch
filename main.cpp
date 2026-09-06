#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <strsafe.h>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include "resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

typedef LONG(NTAPI* pfnNtSuspendProcess)(HANDLE ProcessHandle);
typedef LONG(NTAPI* pfnNtResumeProcess)(HANDLE ProcessHandle);

#define IDC_LBL_TITLE        1000
#define IDC_LBL_SUB          1001
#define IDC_BTN_TRIGGER      1002
#define IDC_BTN_RECORD       1003
#define IDC_BTN_RESET        1004
#define IDC_BTN_TESTSOUND    1005
#define IDC_CHK_SOUND        1006
#define IDC_LBL_CHKSOUND     1007
#define IDC_NUM_DURATION     1008
#define IDC_SPIN_DURATION    1009
#define IDC_PROGRESS         1010
#define IDC_LBL_STATUS       1011
#define IDC_LBL_LOG          1012
#define IDC_LBL_HINT         1013
#define IDC_GRP_HEADER       1014

#define WM_APP_TRAY          (WM_APP + 1)
#define WM_APP_COUNTDOWN     (WM_APP + 2)
#define WM_APP_STATUS        (WM_APP + 3)
#define WM_APP_STATE         (WM_APP + 4)

#define IDM_TRAY_TRIGGER     2001
#define IDM_TRAY_OPEN        2002
#define IDM_TRAY_EXIT        2003

#define IDT_PROCESS_CHECK    3001
#define HOTKEY_ID            9001

static HINSTANCE g_hInstance = NULL;
static HWND g_hWnd = NULL;
static HWND g_hLblTitle = NULL;
static HWND g_hLblSub = NULL;
static HWND g_hBtnTrigger = NULL;
static HWND g_hBtnRecord = NULL;
static HWND g_hBtnReset = NULL;
static HWND g_hBtnTestSound = NULL;
static HWND g_hChkSound = NULL;
static HWND g_hLblChkSound = NULL;
static HWND g_hNumDuration = NULL;
static HWND g_hSpinDuration = NULL;
static HWND g_hProgress = NULL;
static HWND g_hLblStatus = NULL;
static HWND g_hLblLog = NULL;
static HWND g_hLblHint = NULL;
static HWND g_hGrpHeader = NULL;

static HBRUSH g_hBrushBg = NULL;
static HBRUSH g_hBrushPanel = NULL;
static HBRUSH g_hBrushBox = NULL;
static HPEN g_hPenBorder = NULL;

static HFONT g_hFontTitle = NULL;
static HFONT g_hFontSub = NULL;
static HFONT g_hFontBtn = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontNormal = NULL;
static HFONT g_hFontSmall = NULL;

static NOTIFYICONDATAW g_nid = { sizeof(NOTIFYICONDATAW) };
static UINT g_msgRestore = 0;
static HHOOK g_hKeyHook = NULL;

static UINT g_hotkeyMods = 0;
static UINT g_hotkeyVk = VK_PAUSE;
static int g_duration = 10;
static bool g_soundEnabled = true;

static std::atomic<bool> g_isBusy{ false };
static std::atomic<bool> g_isRecording{ false };
static std::atomic<bool> g_stopPolling{ false };
static std::thread g_pollingThread;

static pfnNtSuspendProcess g_NtSuspend = NULL;
static pfnNtResumeProcess g_NtResume = NULL;

static std::wstring GetConfigPath()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    return std::wstring(exePath) + L"config.json";
}

static void LoadSettings()
{
    std::wstring path = GetConfigPath();
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line))
    {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        auto trim = [](std::string& s)
        {
            size_t p1 = s.find_first_not_of(" \t\r\n\",");
            size_t p2 = s.find_last_not_of(" \t\r\n\",");
            if (p1 != std::string::npos && p2 != std::string::npos)
                s = s.substr(p1, p2 - p1 + 1);
            else
                s.clear();
        };
        trim(key);
        trim(val);

        if (key == "Modifiers") g_hotkeyMods = (UINT)std::strtoul(val.c_str(), NULL, 10);
        else if (key == "KeyCode") g_hotkeyVk = (UINT)std::strtoul(val.c_str(), NULL, 10);
        else if (key == "SuspendDurationSeconds") g_duration = std::atoi(val.c_str());
        else if (key == "SoundEnabled") g_soundEnabled = (val == "true" || val == "1");
    }

    if (g_duration < 7) g_duration = 7;
    if (g_duration > 12) g_duration = 12;
    if (g_hotkeyVk == 0) g_hotkeyVk = VK_PAUSE;
}

static void SaveSettings()
{
    std::wstring path = GetConfigPath();
    std::ofstream file(path);
    if (!file.is_open()) return;

    file << "{\n";
    file << "  \"Modifiers\": " << g_hotkeyMods << ",\n";
    file << "  \"KeyCode\": " << g_hotkeyVk << ",\n";
    file << "  \"SuspendDurationSeconds\": " << g_duration << ",\n";
    file << "  \"SoundEnabled\": " << (g_soundEnabled ? "true" : "false") << "\n";
    file << "}\n";
}

static std::wstring FormatKeyName(UINT mods, UINT vk)
{
    std::wstring result;
    if (mods & MOD_CONTROL) result += L"Ctrl + ";
    if (mods & MOD_ALT) result += L"Alt + ";
    if (mods & MOD_SHIFT) result += L"Shift + ";

    switch (vk)
    {
    case VK_PAUSE: result += L"Pause / Break"; break;
    case VK_MULTIPLY: result += L"NumPad *"; break;
    case VK_SUBTRACT: result += L"NumPad -"; break;
    case VK_ADD: result += L"NumPad +"; break;
    case VK_DIVIDE: result += L"NumPad /"; break;
    case VK_DECIMAL: result += L"NumPad ."; break;
    case VK_NUMPAD0: result += L"NumPad 0"; break;
    case VK_NUMPAD1: result += L"NumPad 1"; break;
    case VK_NUMPAD2: result += L"NumPad 2"; break;
    case VK_NUMPAD3: result += L"NumPad 3"; break;
    case VK_NUMPAD4: result += L"NumPad 4"; break;
    case VK_NUMPAD5: result += L"NumPad 5"; break;
    case VK_NUMPAD6: result += L"NumPad 6"; break;
    case VK_NUMPAD7: result += L"NumPad 7"; break;
    case VK_NUMPAD8: result += L"NumPad 8"; break;
    case VK_NUMPAD9: result += L"NumPad 9"; break;
    case VK_INSERT: result += L"Insert"; break;
    case VK_DELETE: result += L"Delete"; break;
    case VK_HOME: result += L"Home"; break;
    case VK_END: result += L"End"; break;
    case VK_PRIOR: result += L"Page Up"; break;
    case VK_NEXT: result += L"Page Down"; break;
    case VK_SCROLL: result += L"Scroll Lock"; break;
    case VK_F1: result += L"F1"; break;
    case VK_F2: result += L"F2"; break;
    case VK_F3: result += L"F3"; break;
    case VK_F4: result += L"F4"; break;
    case VK_F5: result += L"F5"; break;
    case VK_F6: result += L"F6"; break;
    case VK_F7: result += L"F7"; break;
    case VK_F8: result += L"F8"; break;
    case VK_F9: result += L"F9"; break;
    case VK_F10: result += L"F10"; break;
    case VK_F11: result += L"F11"; break;
    case VK_F12: result += L"F12"; break;
    default:
        wchar_t name[64];
        UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        if (scanCode != 0 && GetKeyNameTextW(scanCode << 16, name, 64) > 0)
        {
            result += name;
        }
        else
        {
            wchar_t fallback[16];
            swprintf_s(fallback, L"Key 0x%X", vk);
            result += fallback;
        }
        break;
    }
    return result;
}

static DWORD FindGtaProcess(std::wstring& outName)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    DWORD enhancedPid = 0;
    DWORD standardPid = 0;
    std::wstring enhancedName;
    std::wstring standardName;

    if (Process32FirstW(hSnap, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, L"GTA5_Enhanced.exe") == 0)
            {
                enhancedPid = pe.th32ProcessID;
                enhancedName = pe.szExeFile;
                break;
            }
            else if (_wcsicmp(pe.szExeFile, L"GTA5.exe") == 0)
            {
                standardPid = pe.th32ProcessID;
                standardName = pe.szExeFile;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    if (enhancedPid != 0)
    {
        outName = enhancedName;
        return enhancedPid;
    }
    if (standardPid != 0)
    {
        outName = standardName;
        return standardPid;
    }
    return 0;
}

static void TriggerKillSwitch()
{
    if (g_isBusy.exchange(true)) return;

    PostMessageW(g_hWnd, WM_APP_STATE, 1, 0);

    std::thread([]()
    {
        std::wstring procName;
        DWORD pid = FindGtaProcess(procName);

        if (pid == 0)
        {
            if (g_soundEnabled) Beep(450, 350);
            PostMessageW(g_hWnd, WM_APP_STATUS, 0, 0);
            g_isBusy = false;
            PostMessageW(g_hWnd, WM_APP_STATE, 0, 0);
            return;
        }

        HANDLE hProc = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
        if (!hProc)
        {
            if (g_soundEnabled) Beep(450, 350);
            PostMessageW(g_hWnd, WM_APP_STATUS, 2, 0);
            g_isBusy = false;
            PostMessageW(g_hWnd, WM_APP_STATE, 0, 0);
            return;
        }

        if (g_NtSuspend) g_NtSuspend(hProc);
        if (g_soundEnabled) Beep(900, 250);

        PostMessageW(g_hWnd, WM_APP_STATUS, 1, (LPARAM)pid);

        for (int i = g_duration; i > 0; i--)
        {
            PostMessageW(g_hWnd, WM_APP_COUNTDOWN, (WPARAM)i, 0);
            Sleep(1000);
        }

        if (g_NtResume) g_NtResume(hProc);
        CloseHandle(hProc);

        PostMessageW(g_hWnd, WM_APP_COUNTDOWN, 0, 0);

        if (g_soundEnabled)
        {
            Beep(1050, 120);
            Sleep(70);
            Beep(1350, 180);
        }

        PostMessageW(g_hWnd, WM_APP_STATUS, 3, (LPARAM)pid);
        g_isBusy = false;
        PostMessageW(g_hWnd, WM_APP_STATE, 0, 0);
    }).detach();
}

static void RegisterGlobalHotkey()
{
    UnregisterHotKey(g_hWnd, HOTKEY_ID);
    RegisterHotKey(g_hWnd, HOTKEY_ID, g_hotkeyMods | MOD_NOREPEAT, g_hotkeyVk);

    std::wstring keyStr = FormatKeyName(g_hotkeyMods, g_hotkeyVk);
    SetWindowTextW(g_hBtnRecord, (L"[ " + keyStr + L" ]").c_str());

    std::wstring hint = L"Hotkey [" + keyStr + L"] active in-game (fullscreen & borderless).";
    SetWindowTextW(g_hLblHint, hint.c_str());
}

static void StartKeyPolling()
{
    g_stopPolling = false;
    g_pollingThread = std::thread([]()
    {
        bool wasDown = false;
        auto lastTrigger = std::chrono::steady_clock::now();

        while (!g_stopPolling)
        {
            if (!g_isRecording && !g_isBusy)
            {
                short keyState = GetAsyncKeyState((int)g_hotkeyVk);
                bool isDown = (keyState & 0x8000) != 0;

                if (isDown)
                {
                    if (!wasDown)
                    {
                        bool ctrl = (g_hotkeyMods & MOD_CONTROL) ? ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) : true;
                        bool alt = (g_hotkeyMods & MOD_ALT) ? ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) : true;
                        bool shift = (g_hotkeyMods & MOD_SHIFT) ? ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) : true;

                        if (ctrl && alt && shift)
                        {
                            auto now = std::chrono::steady_clock::now();
                            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTrigger).count() > 1500)
                            {
                                lastTrigger = now;
                                TriggerKillSwitch();
                            }
                        }
                    }
                    wasDown = true;
                }
                else
                {
                    wasDown = false;
                }
            }
            Sleep(20);
        }
    });
}

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && g_isRecording)
    {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
            DWORD vk = pKey->vkCode;

            if (vk != VK_LCONTROL && vk != VK_RCONTROL && vk != VK_CONTROL &&
                vk != VK_LMENU && vk != VK_RMENU && vk != VK_MENU &&
                vk != VK_LSHIFT && vk != VK_RSHIFT && vk != VK_SHIFT &&
                vk != VK_LWIN && vk != VK_RWIN)
            {
                g_isRecording = false;

                if (g_hKeyHook)
                {
                    UnhookWindowsHookEx(g_hKeyHook);
                    g_hKeyHook = NULL;
                }

                if (vk != VK_ESCAPE)
                {
                    UINT mods = 0;
                    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
                    if (GetAsyncKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
                    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;

                    g_hotkeyMods = mods;
                    g_hotkeyVk = vk;
                    SaveSettings();
                    RegisterGlobalHotkey();

                    if (g_soundEnabled) Beep(1200, 100);
                    SetWindowTextW(g_hLblLog, L"New hotkey saved successfully!");
                }
                else
                {
                    RegisterGlobalHotkey();
                    SetWindowTextW(g_hLblLog, L"Hotkey recording cancelled.");
                }

                InvalidateRect(g_hBtnRecord, NULL, TRUE);
                return 1;
            }
        }
    }
    return CallNextHookEx(g_hKeyHook, nCode, wParam, lParam);
}

static void StartRecordingKey()
{
    if (g_isRecording) return;
    g_isRecording = true;

    SetWindowTextW(g_hBtnRecord, L"[ Press any key on keyboard... ]");
    SetWindowTextW(g_hLblHint, L"Press any key (or combo) on keyboard. Press ESC to cancel.");
    InvalidateRect(g_hBtnRecord, NULL, TRUE);

    g_hKeyHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_hInstance, 0);
}

static void RestoreFromTray()
{
    ShowWindow(g_hWnd, SW_RESTORE);
    SetForegroundWindow(g_hWnd);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == g_msgRestore)
    {
        RestoreFromTray();
        return 0;
    }

    switch (uMsg)
    {
    case WM_CREATE:
    {
        g_hBrushBg = CreateSolidBrush(RGB(24, 24, 28));
        g_hBrushPanel = CreateSolidBrush(RGB(36, 36, 44));
        g_hBrushBox = CreateSolidBrush(RGB(28, 28, 34));
        g_hPenBorder = CreatePen(PS_SOLID, 1, RGB(55, 55, 68));

        g_hFontTitle = CreateFontW(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontSub = CreateFontW(14, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontBtn = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontBold = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontNormal = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_hFontSmall = CreateFontW(13, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        g_hLblTitle = CreateWindowW(L"STATIC", L"\x26A1 GTA SOLO KILL SWITCH", WS_CHILD | WS_VISIBLE, 20, 15, 464, 28, hWnd, (HMENU)IDC_LBL_TITLE, g_hInstance, NULL);
        SendMessageW(g_hLblTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

        g_hLblSub = CreateWindowW(L"STATIC", L"One-click / hotkey emergency switch to create a solo public session", WS_CHILD | WS_VISIBLE, 22, 45, 464, 20, hWnd, (HMENU)IDC_LBL_SUB, g_hInstance, NULL);
        SendMessageW(g_hLblSub, WM_SETFONT, (WPARAM)g_hFontSub, TRUE);

        g_hLblStatus = CreateWindowW(L"STATIC", L"  \x25CB Scanning for GTA V...", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 20, 75, 464, 45, hWnd, (HMENU)IDC_LBL_STATUS, g_hInstance, NULL);
        SendMessageW(g_hLblStatus, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        wchar_t btnText[64];
        swprintf_s(btnText, L"\x26A1 GO SOLO SESSION (%ds)", g_duration);
        g_hBtnTrigger = CreateWindowW(L"BUTTON", btnText, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 20, 132, 464, 60, hWnd, (HMENU)IDC_BTN_TRIGGER, g_hInstance, NULL);
        SendMessageW(g_hBtnTrigger, WM_SETFONT, (WPARAM)g_hFontBtn, TRUE);

        g_hProgress = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | PBS_SMOOTH, 20, 202, 464, 10, hWnd, (HMENU)IDC_PROGRESS, g_hInstance, NULL);
        SendMessageW(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, g_duration));

        g_hGrpHeader = CreateWindowW(L"STATIC", L"  SETTINGS & HOTKEY  ", WS_CHILD | WS_VISIBLE, 35, 220, 180, 18, hWnd, (HMENU)IDC_GRP_HEADER, g_hInstance, NULL);
        SendMessageW(g_hGrpHeader, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        HWND hLblKey = CreateWindowW(L"STATIC", L"Emergency Hotkey:", WS_CHILD | WS_VISIBLE, 35, 255, 140, 22, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hLblKey, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hBtnRecord = CreateWindowW(L"BUTTON", L"[ Pause / Break ]", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 185, 250, 200, 32, hWnd, (HMENU)IDC_BTN_RECORD, g_hInstance, NULL);
        SendMessageW(g_hBtnRecord, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        g_hBtnReset = CreateWindowW(L"BUTTON", L"Reset", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 395, 250, 70, 32, hWnd, (HMENU)IDC_BTN_RESET, g_hInstance, NULL);
        SendMessageW(g_hBtnReset, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hLblHint = CreateWindowW(L"STATIC", L"Click the hotkey box above and press any key to rebind.", WS_CHILD | WS_VISIBLE, 35, 290, 430, 18, hWnd, (HMENU)IDC_LBL_HINT, g_hInstance, NULL);
        SendMessageW(g_hLblHint, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

        HWND hLblDur = CreateWindowW(L"STATIC", L"Freeze Duration:", WS_CHILD | WS_VISIBLE, 35, 320, 130, 22, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hLblDur, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        wchar_t durText[8];
        swprintf_s(durText, L"%d", g_duration);
        g_hNumDuration = CreateWindowW(L"EDIT", durText, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER, 185, 318, 50, 25, hWnd, (HMENU)IDC_NUM_DURATION, g_hInstance, NULL);
        SendMessageW(g_hNumDuration, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        g_hSpinDuration = CreateWindowW(UPDOWN_CLASSW, NULL, WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0, hWnd, (HMENU)IDC_SPIN_DURATION, g_hInstance, NULL);
        SendMessageW(g_hSpinDuration, UDM_SETBUDDY, (WPARAM)g_hNumDuration, 0);
        SendMessageW(g_hSpinDuration, UDM_SETRANGE, 0, MAKELPARAM(12, 7));
        SendMessageW(g_hSpinDuration, UDM_SETPOS, 0, MAKELPARAM(g_duration, 0));

        HWND hLblSec = CreateWindowW(L"STATIC", L"seconds (max 12s, 10s recommended)", WS_CHILD | WS_VISIBLE, 245, 321, 230, 20, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hLblSec, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

        g_hChkSound = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 35, 357, 18, 18, hWnd, (HMENU)IDC_CHK_SOUND, g_hInstance, NULL);
        SendMessageW(g_hChkSound, BM_SETCHECK, g_soundEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

        g_hLblChkSound = CreateWindowW(L"STATIC", L"Play audio cue on freeze and resume", WS_CHILD | WS_VISIBLE | SS_NOTIFY, 58, 357, 315, 20, hWnd, (HMENU)IDC_LBL_CHKSOUND, g_hInstance, NULL);
        SendMessageW(g_hLblChkSound, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_hBtnTestSound = CreateWindowW(L"BUTTON", L"Test Beep", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 380, 352, 85, 28, hWnd, (HMENU)IDC_BTN_TESTSOUND, g_hInstance, NULL);
        SendMessageW(g_hBtnTestSound, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

        HWND hInfo = CreateWindowW(L"STATIC", L"Minimizing this window sends it to the system tray.", WS_CHILD | WS_VISIBLE, 35, 395, 430, 18, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hInfo, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

        g_hLblLog = CreateWindowW(L"STATIC", L"Ready. Press your hotkey in-game to trigger a solo session.", WS_CHILD | WS_VISIBLE | SS_CENTER, 20, 455, 464, 35, hWnd, (HMENU)IDC_LBL_LOG, g_hInstance, NULL);
        SendMessageW(g_hLblLog, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        g_nid.hWnd = hWnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_APP_TRAY;
        g_nid.hIcon = LoadIconW(g_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
        StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"GTA Online - Kill Switch");
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        RegisterGlobalHotkey();
        StartKeyPolling();

        SetTimer(hWnd, IDT_PROCESS_CHECK, 1500, NULL);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        SelectObject(hdc, g_hPenBorder);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, 20, 228, 484, 438);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_TIMER:
    {
        if (wParam == IDT_PROCESS_CHECK)
        {
            if (g_isBusy) return 0;

            std::wstring procName;
            DWORD pid = FindGtaProcess(procName);
            if (pid != 0)
            {
                wchar_t status[128];
                swprintf_s(status, L"  \x25CF %s (PID: %lu) detected - Ready!", procName.c_str(), pid);
                SetWindowTextW(g_hLblStatus, status);
            }
            else
            {
                SetWindowTextW(g_hLblStatus, L"  \x25CB GTA V is not running. Waiting for game...");
            }
        }
        return 0;
    }

    case WM_HOTKEY:
    {
        if (wParam == HOTKEY_ID && !g_isRecording)
        {
            TriggerKillSwitch();
        }
        return 0;
    }

    case WM_APP_STATE:
    {
        bool busy = (wParam == 1);
        EnableWindow(g_hBtnTrigger, !busy);
        if (busy)
        {
            ShowWindow(g_hProgress, SW_SHOW);
            SendMessageW(g_hProgress, PBM_SETPOS, g_duration, 0);
            InvalidateRect(g_hBtnTrigger, NULL, TRUE);
        }
        else
        {
            ShowWindow(g_hProgress, SW_HIDE);
            wchar_t btnText[64];
            swprintf_s(btnText, L"\x26A1 GO SOLO SESSION (%ds)", g_duration);
            SetWindowTextW(g_hBtnTrigger, btnText);
            InvalidateRect(g_hBtnTrigger, NULL, TRUE);
        }
        return 0;
    }

    case WM_APP_COUNTDOWN:
    {
        int count = (int)wParam;
        if (count > 0)
        {
            wchar_t btnText[64];
            swprintf_s(btnText, L"FREEZING GTA V: %ds remaining...", count);
            SetWindowTextW(g_hBtnTrigger, btnText);
            SendMessageW(g_hProgress, PBM_SETPOS, count, 0);

            wchar_t logText[128];
            swprintf_s(logText, L"Disconnecting other players... %d second%s remaining", count, count > 1 ? L"s" : L"");
            SetWindowTextW(g_hLblLog, logText);
        }
        else
        {
            SendMessageW(g_hProgress, PBM_SETPOS, 0, 0);
        }
        return 0;
    }

    case WM_APP_STATUS:
    {
        int code = (int)wParam;
        if (code == 0)
        {
            SetWindowTextW(g_hLblLog, L"GTA V process not found. Launch GTA V first.");
        }
        else if (code == 1)
        {
            SetWindowTextW(g_hLblStatus, L"  \x26A1 GTA V SUSPENDED (CREATING SOLO SESSION)");
            SetWindowTextW(g_hLblLog, L"GTA V suspended... Disconnecting peers!");
        }
        else if (code == 2)
        {
            SetWindowTextW(g_hLblLog, L"Access denied to GTA V process.");
        }
        else if (code == 3)
        {
            SetWindowTextW(g_hLblLog, L"GTA V resumed! You are now in your solo public session.");
        }
        return 0;
    }

    case WM_COMMAND:
    {
        WORD id = LOWORD(wParam);
        WORD code = HIWORD(wParam);

        if (id == IDC_BTN_TRIGGER)
        {
            TriggerKillSwitch();
        }
        else if (id == IDC_BTN_RECORD)
        {
            StartRecordingKey();
        }
        else if (id == IDC_BTN_RESET)
        {
            g_hotkeyMods = 0;
            g_hotkeyVk = VK_PAUSE;
            SaveSettings();
            RegisterGlobalHotkey();
            SetWindowTextW(g_hLblLog, L"Reset hotkey to default: [Pause / Break]");
        }
        else if (id == IDC_CHK_SOUND)
        {
            g_soundEnabled = (SendMessageW(g_hChkSound, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SaveSettings();
        }
        else if (id == IDC_LBL_CHKSOUND && code == STN_CLICKED)
        {
            g_soundEnabled = !g_soundEnabled;
            SendMessageW(g_hChkSound, BM_SETCHECK, g_soundEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
            SaveSettings();
        }
        else if (id == IDC_BTN_TESTSOUND)
        {
            std::thread([]()
            {
                Beep(900, 250);
                Sleep(800);
                Beep(1050, 120);
                Sleep(70);
                Beep(1350, 180);
            }).detach();
        }
        else if (id == IDC_NUM_DURATION && code == EN_CHANGE)
        {
            wchar_t buf[16];
            GetWindowTextW(g_hNumDuration, buf, 16);
            int val = _wtoi(buf);
            if (val >= 7 && val <= 12 && val != g_duration)
            {
                g_duration = val;
                SaveSettings();
                SendMessageW(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, g_duration));
                if (!g_isBusy)
                {
                    wchar_t bText[64];
                    swprintf_s(bText, L"\x26A1 GO SOLO SESSION (%ds)", g_duration);
                    SetWindowTextW(g_hBtnTrigger, bText);
                }
            }
        }
        else if (id == IDC_NUM_DURATION && code == EN_KILLFOCUS)
        {
            wchar_t buf[16];
            GetWindowTextW(g_hNumDuration, buf, 16);
            int val = _wtoi(buf);
            if (val < 7) val = 7;
            if (val > 12) val = 12;
            if (val != g_duration)
            {
                g_duration = val;
                SaveSettings();
                SendMessageW(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, g_duration));
                if (!g_isBusy)
                {
                    wchar_t bText[64];
                    swprintf_s(bText, L"\x26A1 GO SOLO SESSION (%ds)", g_duration);
                    SetWindowTextW(g_hBtnTrigger, bText);
                }
            }
            swprintf_s(buf, L"%d", g_duration);
            SetWindowTextW(g_hNumDuration, buf);
        }
        return 0;
    }

    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
        if (pdis->CtlID == IDC_BTN_TRIGGER)
        {
            HBRUSH hBrush = g_isBusy ? CreateSolidBrush(RGB(90, 90, 100)) : CreateSolidBrush(RGB(230, 81, 0));
            FillRect(pdis->hDC, &pdis->rcItem, hBrush);
            DeleteObject(hBrush);

            SetBkMode(pdis->hDC, TRANSPARENT);
            SetTextColor(pdis->hDC, g_isBusy ? RGB(255, 215, 0) : RGB(255, 255, 255));
            SelectObject(pdis->hDC, g_hFontBtn);

            wchar_t text[64];
            GetWindowTextW(g_hBtnTrigger, text, 64);
            DrawTextW(pdis->hDC, text, -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        else if (pdis->CtlID == IDC_BTN_RECORD)
        {
            HBRUSH hBrush = g_isRecording ? CreateSolidBrush(RGB(255, 180, 0)) : CreateSolidBrush(RGB(45, 45, 55));
            FillRect(pdis->hDC, &pdis->rcItem, hBrush);
            DeleteObject(hBrush);

            HPEN hPen = CreatePen(PS_SOLID, 1, g_isRecording ? RGB(255, 215, 0) : RGB(76, 175, 80));
            HPEN hOldPen = (HPEN)SelectObject(pdis->hDC, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(pdis->hDC, GetStockObject(NULL_BRUSH));
            Rectangle(pdis->hDC, pdis->rcItem.left, pdis->rcItem.top, pdis->rcItem.right, pdis->rcItem.bottom);
            SelectObject(pdis->hDC, hOldBrush);
            SelectObject(pdis->hDC, hOldPen);
            DeleteObject(hPen);

            SetBkMode(pdis->hDC, TRANSPARENT);
            SetTextColor(pdis->hDC, g_isRecording ? RGB(10, 10, 10) : RGB(76, 215, 80));
            SelectObject(pdis->hDC, g_hFontBold);

            wchar_t text[64];
            GetWindowTextW(g_hBtnRecord, text, 64);
            DrawTextW(pdis->hDC, text, -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        else if (pdis->CtlID == IDC_BTN_RESET || pdis->CtlID == IDC_BTN_TESTSOUND)
        {
            HBRUSH hBrush = (pdis->itemState & ODS_SELECTED) ? CreateSolidBrush(RGB(40, 40, 48)) : CreateSolidBrush(RGB(50, 50, 60));
            FillRect(pdis->hDC, &pdis->rcItem, hBrush);
            DeleteObject(hBrush);

            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(75, 75, 90));
            HPEN hOldPen = (HPEN)SelectObject(pdis->hDC, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(pdis->hDC, GetStockObject(NULL_BRUSH));
            Rectangle(pdis->hDC, pdis->rcItem.left, pdis->rcItem.top, pdis->rcItem.right, pdis->rcItem.bottom);
            SelectObject(pdis->hDC, hOldBrush);
            SelectObject(pdis->hDC, hOldPen);
            DeleteObject(hPen);

            SetBkMode(pdis->hDC, TRANSPARENT);
            SetTextColor(pdis->hDC, RGB(235, 235, 240));
            SelectObject(pdis->hDC, (pdis->CtlID == IDC_BTN_TESTSOUND) ? g_hFontSmall : g_hFontNormal);

            wchar_t text[32];
            GetWindowTextW(pdis->hwndItem, text, 32);
            DrawTextW(pdis->hDC, text, -1, &pdis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        HWND hCtl = (HWND)lParam;

        SetBkMode(hdc, TRANSPARENT);

        if (hCtl == g_hLblTitle)
        {
            SetTextColor(hdc, RGB(255, 180, 0));
            return (LRESULT)g_hBrushBg;
        }
        if (hCtl == g_hLblSub)
        {
            SetTextColor(hdc, RGB(180, 180, 190));
            return (LRESULT)g_hBrushBg;
        }
        if (hCtl == g_hGrpHeader)
        {
            SetTextColor(hdc, RGB(255, 180, 0));
            return (LRESULT)g_hBrushBg;
        }
        if (hCtl == g_hLblStatus)
        {
            if (g_isBusy) SetTextColor(hdc, RGB(255, 215, 0));
            else SetTextColor(hdc, RGB(76, 215, 80));
            return (LRESULT)g_hBrushPanel;
        }
        if (hCtl == g_hLblLog)
        {
            SetTextColor(hdc, RGB(225, 225, 235));
            return (LRESULT)g_hBrushBg;
        }
        if (hCtl == g_hLblHint)
        {
            SetTextColor(hdc, g_isRecording ? RGB(255, 215, 0) : RGB(170, 170, 185));
            return (LRESULT)g_hBrushBg;
        }
        if (hCtl == g_hLblChkSound)
        {
            SetTextColor(hdc, RGB(240, 240, 245));
            return (LRESULT)g_hBrushBg;
        }

        SetTextColor(hdc, RGB(240, 240, 245));
        return (LRESULT)g_hBrushBg;
    }

    case WM_CTLCOLORBTN:
    {
        return (LRESULT)g_hBrushBg;
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, RGB(45, 45, 55));
        SetTextColor(hdc, RGB(255, 255, 255));
        return (LRESULT)g_hBrushPanel;
    }

    case WM_SIZE:
    {
        if (wParam == SIZE_MINIMIZED)
        {
            ShowWindow(hWnd, SW_HIDE);
            g_nid.uFlags = NIF_INFO;
            StringCchCopyW(g_nid.szInfoTitle, ARRAYSIZE(g_nid.szInfoTitle), L"GTA Kill Switch");
            StringCchCopyW(g_nid.szInfo, ARRAYSIZE(g_nid.szInfo), L"Running in background. Press your hotkey in-game!");
            g_nid.dwInfoFlags = NIIF_INFO;
            Shell_NotifyIconW(NIM_MODIFY, &g_nid);
            g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        }
        return 0;
    }

    case WM_APP_TRAY:
    {
        if (lParam == WM_LBUTTONDBLCLK)
        {
            RestoreFromTray();
        }
        else if (lParam == WM_RBUTTONUP)
        {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_TRAY_TRIGGER, L"Go Solo Session Now");
            InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
            InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, IDM_TRAY_OPEN, L"Open Window");
            InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_STRING, IDM_TRAY_EXIT, L"Exit");

            SetForegroundWindow(hWnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);

            if (cmd == IDM_TRAY_TRIGGER) TriggerKillSwitch();
            else if (cmd == IDM_TRAY_OPEN) RestoreFromTray();
            else if (cmd == IDM_TRAY_EXIT) DestroyWindow(hWnd);
        }
        return 0;
    }

    case WM_DESTROY:
    {
        KillTimer(hWnd, IDT_PROCESS_CHECK);
        g_stopPolling = true;
        if (g_pollingThread.joinable()) g_pollingThread.join();

        if (g_hKeyHook) UnhookWindowsHookEx(g_hKeyHook);
        UnregisterHotKey(hWnd, HOTKEY_ID);

        Shell_NotifyIconW(NIM_DELETE, &g_nid);

        if (g_hBrushBg) DeleteObject(g_hBrushBg);
        if (g_hBrushPanel) DeleteObject(g_hBrushPanel);
        if (g_hBrushBox) DeleteObject(g_hBrushBox);
        if (g_hPenBorder) DeleteObject(g_hPenBorder);

        if (g_hFontTitle) DeleteObject(g_hFontTitle);
        if (g_hFontSub) DeleteObject(g_hFontSub);
        if (g_hFontBtn) DeleteObject(g_hFontBtn);
        if (g_hFontBold) DeleteObject(g_hFontBold);
        if (g_hFontNormal) DeleteObject(g_hFontNormal);
        if (g_hFontSmall) DeleteObject(g_hFontSmall);

        PostQuitMessage(0);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"GTAKillSwitch_Mutex_7E8B99FA");
    g_msgRestore = RegisterWindowMessageW(L"GTAKillSwitch_Restore_7E8B99FA");

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        PostMessageW(HWND_BROADCAST, g_msgRestore, 0, 0);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll)
    {
        g_NtSuspend = (pfnNtSuspendProcess)GetProcAddress(hNtdll, "NtSuspendProcess");
        g_NtResume = (pfnNtResumeProcess)GetProcAddress(hNtdll, "NtResumeProcess");
    }

    InitCommonControls();
    g_hInstance = hInstance;
    LoadSettings();

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(24, 24, 28));
    wc.lpszClassName = L"GTAKillSwitchClass";

    if (!RegisterClassExW(&wc)) return 1;

    RECT rc = { 0, 0, 505, 510 };
    AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);

    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    g_hWnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"GTA Online - Solo Session Kill Switch",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        (screenW - width) / 2,
        (screenH - height) / 2,
        width,
        height,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!g_hWnd) return 1;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hMutex)
    {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return (int)msg.wParam;
}
