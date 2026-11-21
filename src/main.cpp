#define OEMRESOURCE
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "resource.h"
#include "shake_detector.h"
#include "cursor_renderer.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdiplus.lib")

// ---------------------------------------------------------------------------
// Global Constants & State
// ---------------------------------------------------------------------------
static const wchar_t* const APP_NAME = L"Cursor_Vansh";
static const wchar_t* const APP_MUTEX_NAME = L"Global\\Cursor_Vansh_SingleInstance_Mutex";
static const wchar_t* const REG_RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const UINT_PTR TIMER_ANIMATION_ID = 1001;

enum class AnimState {
    Idle,
    Growing,
    Holding,
    Shrinking
};

struct AppConfig {
    ShakeDetector::Sensitivity sensitivity = ShakeDetector::Sensitivity::Medium;
    CursorStyle style = CursorStyle::MacClassic;
    float max_scale = 3.6f;
    int grow_ms = 130;
    int hold_ms = 220;
    int shrink_ms = 240;
    bool pause_in_fullscreen = false;
};

static AppConfig g_config;
static ShakeDetector g_detector(g_config.sensitivity);
static CursorRenderer g_renderer;
static ULONG_PTR g_gdiplus_token = 0;

static HWND g_main_hwnd = nullptr;      // Hidden window for tray icon and message loop
static HWND g_overlay_hwnd = nullptr;   // Transparent layered click-through overlay window
static HHOOK g_mouse_hook = nullptr;
static NOTIFYICONDATAW g_nid = { 0 };

static AnimState g_anim_state = AnimState::Idle;
static ULONGLONG g_anim_start_tick = 0;
static ULONGLONG g_last_shake_tick = 0;
static float g_current_scale = 1.0f;
static BYTE g_current_alpha = 255;

// ---------------------------------------------------------------------------
// Helper: Get Config File Path
// ---------------------------------------------------------------------------
std::wstring GetConfigFilePath() {
    wchar_t local_app_data[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local_app_data))) {
        std::wstring dir = std::wstring(local_app_data) + L"\\" + APP_NAME;
        CreateDirectoryW(dir.c_str(), NULL);
        return dir + L"\\config.ini";
    }
    return L"config.ini";
}

void LoadConfig() {
    std::wstring path = GetConfigFilePath();
    int sens = GetPrivateProfileIntW(L"Settings", L"Sensitivity", static_cast<int>(g_config.sensitivity), path.c_str());
    if (sens >= 0 && sens <= 2) {
        g_config.sensitivity = static_cast<ShakeDetector::Sensitivity>(sens);
        g_detector.SetSensitivity(g_config.sensitivity);
    }

    int style = GetPrivateProfileIntW(L"Settings", L"Style", static_cast<int>(g_config.style), path.c_str());
    if (style >= 0 && style <= 2) {
        g_config.style = static_cast<CursorStyle>(style);
        g_renderer.SetStyle(g_config.style);
    }

    g_config.pause_in_fullscreen = (GetPrivateProfileIntW(L"Settings", L"PauseInFullscreen", 0, path.c_str()) != 0);
}

void SaveConfig() {
    std::wstring path = GetConfigFilePath();
    WritePrivateProfileStringW(L"Settings", L"Sensitivity", std::to_wstring(static_cast<int>(g_config.sensitivity)).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"Style", std::to_wstring(static_cast<int>(g_config.style)).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"PauseInFullscreen", g_config.pause_in_fullscreen ? L"1" : L"0", path.c_str());
}

// ---------------------------------------------------------------------------
// Auto-Start (Run Key in HKCU)
// ---------------------------------------------------------------------------
bool IsAutoStartEnabled() {
    HKEY hkey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0, KEY_READ, &hkey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0;
    DWORD size = 0;
    LONG res = RegQueryValueExW(hkey, APP_NAME, nullptr, &type, nullptr, &size);
    RegCloseKey(hkey);
    return (res == ERROR_SUCCESS);
}

bool SetAutoStart(bool enable) {
    HKEY hkey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0, KEY_SET_VALUE, &hkey) != ERROR_SUCCESS) {
        return false;
    }

    LONG res = ERROR_SUCCESS;
    if (enable) {
        wchar_t exe_path[MAX_PATH];
        GetModuleFileNameW(NULL, exe_path, MAX_PATH);
        std::wstring quoted = L"\"" + std::wstring(exe_path) + L"\"";
        res = RegSetValueExW(hkey, APP_NAME, 0, REG_SZ, 
                             reinterpret_cast<const BYTE*>(quoted.c_str()), 
                             static_cast<DWORD>((quoted.length() + 1) * sizeof(wchar_t)));
    } else {
        res = RegDeleteValueW(hkey, APP_NAME);
    }

    RegCloseKey(hkey);
    return (res == ERROR_SUCCESS);
}

// ---------------------------------------------------------------------------
// Cursor Visibility & Gaming Detection
// ---------------------------------------------------------------------------
bool IsDefaultCursorVisible() {
    CURSORINFO ci = { sizeof(CURSORINFO) };
    if (!GetCursorInfo(&ci)) {
        // Fallback: attach to active input desktop if called from background thread
        HDESK hDesk = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS | GENERIC_READ);
        if (hDesk) {
            SetThreadDesktop(hDesk);
            CloseDesktop(hDesk);
            if (!GetCursorInfo(&ci)) {
                return false;
            }
        } else {
            return false;
        }
    }

    // 1. Must be currently visible (not hidden by ShowCursor(FALSE) in games)
    if ((ci.flags & CURSOR_SHOWING) == 0) {
        return false;
    }

    // 2. Must not be suppressed by touch or stylus
    if (ci.flags & 2 /* CURSOR_SUPPRESSED */) {
        return false;
    }

    // 3. Must have a valid cursor handle
    if (ci.hCursor == NULL) {
        return false;
    }

    // 4. Must not be trapped in a tiny clip box (games lock mouse to a 1x1 or tiny box for camera control)
    RECT clip_rect = { 0 };
    if (GetClipCursor(&clip_rect)) {
        int w = clip_rect.right - clip_rect.left;
        int h = clip_rect.bottom - clip_rect.top;
        if (w > 0 && h > 0 && w <= 4 && h <= 4) {
            return false;
        }
    }

    // 5. Must not be a dummy 1x1 blank cursor (some games set a transparent 1x1 dummy cursor)
    static HCURSOR s_last_cursor = NULL;
    static bool s_last_valid = true;
    if (ci.hCursor != s_last_cursor) {
        s_last_cursor = ci.hCursor;
        s_last_valid = true;

        ICONINFO ii = { 0 };
        if (GetIconInfo(ci.hCursor, &ii)) {
            BITMAP bm = { 0 };
            HBITMAP hbm = ii.hbmColor ? ii.hbmColor : ii.hbmMask;
            if (hbm && GetObject(hbm, sizeof(BITMAP), &bm)) {
                if (bm.bmWidth <= 1 && bm.bmHeight <= 1) {
                    s_last_valid = false;
                }
            }
            if (ii.hbmColor) DeleteObject(ii.hbmColor);
            if (ii.hbmMask) DeleteObject(ii.hbmMask);
        }
    }

    return s_last_valid;
}

bool IsForegroundWindowFullscreen() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd || hwnd == GetDesktopWindow() || hwnd == GetShellWindow()) {
        return false;
    }

    RECT appBounds;
    if (GetWindowRect(hwnd, &appBounds)) {
        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (GetMonitorInfo(hMon, &mi)) {
            bool isFull = (appBounds.left <= mi.rcMonitor.left &&
                           appBounds.top <= mi.rcMonitor.top &&
                           appBounds.right >= mi.rcMonitor.right &&
                           appBounds.bottom >= mi.rcMonitor.bottom);
            if (isFull) {
                wchar_t className[256] = { 0 };
                GetClassNameW(hwnd, className, 256);
                if (_wcsicmp(className, L"Progman") == 0 || 
                    _wcsicmp(className, L"WorkerW") == 0 ||
                    _wcsicmp(className, L"Shell_TrayWnd") == 0) {
                    return false;
                }
                return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Low-Level Mouse Hook (WH_MOUSE_LL)
// ---------------------------------------------------------------------------
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_MOUSEMOVE) {
        const MSLLHOOKSTRUCT* pMouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        if (pMouse) {
            DWORD time_ms = pMouse->time;
            if (time_ms == 0) time_ms = GetTickCount();

            // When idle, only track movements if the default cursor is actively visible on screen!
            if (g_anim_state == AnimState::Idle) {
                if (!IsDefaultCursorVisible() || (g_config.pause_in_fullscreen && IsForegroundWindowFullscreen())) {
                    g_detector.Reset(); // Clear movement buffer so game aim moves are ignored!
                    return CallNextHookEx(g_mouse_hook, nCode, wParam, lParam);
                }
            }

            if (g_detector.OnMouseMove(pMouse->pt.x, pMouse->pt.y, time_ms)) {
                // Post shake message to main thread, keeping hook execution instantaneous!
                if (g_main_hwnd) {
                    PostMessageW(g_main_hwnd, WM_SHAKE_TRIGGERED, 0, 0);
                }
            } else if (g_anim_state != AnimState::Idle) {
                // While animation is active, track moves to keep holding or repositioning
                g_last_shake_tick = GetTickCount64();
            }
        }
    }
    return CallNextHookEx(g_mouse_hook, nCode, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Hardware System Cursor Management (Hides native pointer during enlargement)
// ---------------------------------------------------------------------------
static bool g_system_cursor_hidden = false;

static const DWORD kSystemCursorIds[] = {
    OCR_NORMAL,       // 32512 - Standard arrow
    OCR_IBEAM,        // 32513 - I-beam
    OCR_WAIT,         // 32514 - Wait / hourglass
    OCR_CROSS,        // 32515 - Crosshair
    OCR_UP,           // 32516 - Up arrow
    OCR_SIZENWSE,     // 32642 - Resize NW-SE
    OCR_SIZENESW,     // 32643 - Resize NE-SW
    OCR_SIZEWE,       // 32644 - Resize W-E
    OCR_SIZENS,       // 32645 - Resize N-S
    OCR_SIZEALL,      // 32646 - Move all
    OCR_NO,           // 32648 - Slashed circle
    OCR_HAND,         // 32649 - Hand / link
    OCR_APPSTARTING   // 32650 - App starting
};

void HideSystemCursor() {
    if (g_system_cursor_hidden) return;

    int w = GetSystemMetrics(SM_CXCURSOR);
    int h = GetSystemMetrics(SM_CYCURSOR);
    if (w <= 0 || h <= 0) { w = 32; h = 32; }

    int row_bytes = ((w + 15) / 16) * 2;
    int mask_size = row_bytes * h;
    std::vector<BYTE> and_mask(mask_size, 0xFF); // 1 = transparent
    std::vector<BYTE> xor_mask(mask_size, 0x00);

    for (DWORD id : kSystemCursorIds) {
        HCURSOR hBlank = CreateCursor(GetModuleHandle(NULL), 0, 0, w, h, and_mask.data(), xor_mask.data());
        if (hBlank) {
            SetSystemCursor(hBlank, id); // SetSystemCursor takes ownership and destroys hBlank
        }
    }
    g_system_cursor_hidden = true;
}

void RestoreSystemCursor() {
    if (!g_system_cursor_hidden) return;
    SystemParametersInfo(SPI_SETCURSORS, 0, NULL, 0);
    g_system_cursor_hidden = false;
}

// ---------------------------------------------------------------------------
// Overlay Window Rendering & Animation
// ---------------------------------------------------------------------------
void UpdateAnimationTick();

void TriggerShakeAnimation() {
    ULONGLONG now = GetTickCount64();
    g_last_shake_tick = now;

    if (g_anim_state == AnimState::Idle) {
        if (!IsDefaultCursorVisible() || (g_config.pause_in_fullscreen && IsForegroundWindowFullscreen())) {
            g_detector.Reset();
            return;
        }

        // Hide the native hardware cursor so our enlarged vector cursor is completely in front!
        HideSystemCursor();

        g_anim_state = AnimState::Growing;
        g_anim_start_tick = now;
        g_current_scale = 1.0f;
        g_current_alpha = 255;

        // Position overlay window at current mouse cursor
        POINT pt;
        GetCursorPos(&pt);
        SetWindowPos(g_overlay_hwnd, HWND_TOPMOST, 
                     pt.x - CursorRenderer::HOTSPOT_OFFSET, 
                     pt.y - CursorRenderer::HOTSPOT_OFFSET, 
                     CursorRenderer::BUFFER_SIZE, CursorRenderer::BUFFER_SIZE, 
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);

        // Render first frame immediately for instant responsiveness
        UpdateAnimationTick();

        // Start 60 FPS animation timer
        SetTimer(g_overlay_hwnd, TIMER_ANIMATION_ID, 16, nullptr);
    } else if (g_anim_state == AnimState::Shrinking) {
        // Reverse direction back to holding/growing
        g_anim_state = AnimState::Holding;
        g_anim_start_tick = now;
    } else if (g_anim_state == AnimState::Holding) {
        // Extend hold duration
        g_anim_start_tick = now;
    }
}

void UpdateAnimationTick() {
    if (g_anim_state == AnimState::Idle) return;

    ULONGLONG now = GetTickCount64();
    POINT pt;
    GetCursorPos(&pt);

    switch (g_anim_state) {
    case AnimState::Growing: {
        int elapsed = static_cast<int>(now - g_anim_start_tick);
        if (elapsed >= g_config.grow_ms) {
            g_current_scale = g_config.max_scale;
            g_current_alpha = 255;
            g_anim_state = AnimState::Holding;
            g_anim_start_tick = now;
        } else {
            float t = static_cast<float>(elapsed) / static_cast<float>(g_config.grow_ms);
            // Ease-out cubic: snappy explosive growth
            float eased = 1.0f - std::pow(1.0f - t, 3.0f);
            g_current_scale = 1.0f + (g_config.max_scale - 1.0f) * eased;
            g_current_alpha = 255;
        }
        break;
    }
    case AnimState::Holding: {
        g_current_scale = g_config.max_scale;
        g_current_alpha = 255;
        int elapsed = static_cast<int>(now - g_anim_start_tick);
        int since_last_shake = static_cast<int>(now - g_last_shake_tick);

        // Stay holding while user is still shaking or actively moving
        if (elapsed >= g_config.hold_ms && since_last_shake >= 150) {
            g_anim_state = AnimState::Shrinking;
            g_anim_start_tick = now;
        }
        break;
    }
    case AnimState::Shrinking: {
        int elapsed = static_cast<int>(now - g_anim_start_tick);
        if (elapsed >= g_config.shrink_ms) {
            RestoreSystemCursor();
            g_anim_state = AnimState::Idle;
            ShowWindow(g_overlay_hwnd, SW_HIDE);
            KillTimer(g_overlay_hwnd, TIMER_ANIMATION_ID);
            return;
        } else {
            float t = static_cast<float>(elapsed) / static_cast<float>(g_config.shrink_ms);
            // Ease-in quadratic: smooth decay
            float eased = t * t;
            g_current_scale = g_config.max_scale - (g_config.max_scale - 1.0f) * eased;

            // In the last 25% of shrink, seamlessly fade alpha out
            if (t > 0.75f) {
                float fade_t = (t - 0.75f) / 0.25f;
                g_current_alpha = static_cast<BYTE>(255 * (1.0f - fade_t));
            } else {
                g_current_alpha = 255;
            }
        }
        break;
    }
    default:
        break;
    }

    // Render into memory DC
    g_renderer.Render(g_current_scale, g_current_alpha);

    // Update layered window position and surface
    POINT ptDst = { pt.x - CursorRenderer::HOTSPOT_OFFSET, pt.y - CursorRenderer::HOTSPOT_OFFSET };
    SIZE sizeWnd = { CursorRenderer::BUFFER_SIZE, CursorRenderer::BUFFER_SIZE };
    POINT ptSrc = { 0, 0 };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    HDC screen_dc = GetDC(NULL);
    UpdateLayeredWindow(g_overlay_hwnd, screen_dc, &ptDst, &sizeWnd, 
                        g_renderer.GetDC(), &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(NULL, screen_dc);
}

// ---------------------------------------------------------------------------
// Overlay Window Procedure
// ---------------------------------------------------------------------------
LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER:
        if (wParam == TIMER_ANIMATION_ID) {
            UpdateAnimationTick();
        }
        return 0;
    case WM_NCHITTEST:
        return HTTRANSPARENT; // Never hit-test; 100% click-through
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ---------------------------------------------------------------------------
// System Tray Menu & Hidden Window Procedure
// ---------------------------------------------------------------------------
void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_DISABLED, IDM_TITLE, L"Cursor_Vansh (Active)");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_TEST_SHAKE, L"Test Cursor Shake Effect");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);

    // Auto-start toggle
    UINT autostart_flags = MF_BYPOSITION | MF_STRING;
    if (IsAutoStartEnabled()) autostart_flags |= MF_CHECKED;
    InsertMenuW(hMenu, -1, autostart_flags, IDM_AUTOSTART, L"Start Automatically with Windows");

    // Pause in fullscreen games toggle
    UINT fs_flags = MF_BYPOSITION | MF_STRING;
    if (g_config.pause_in_fullscreen) fs_flags |= MF_CHECKED;
    InsertMenuW(hMenu, -1, fs_flags, IDM_PAUSE_FULLSCREEN, L"Pause in Fullscreen Games");

    // Cursor Style Submenu
    HMENU hStyleMenu = CreatePopupMenu();
    UINT mac_flags = MF_BYPOSITION | MF_STRING | (g_config.style == CursorStyle::MacClassic ? MF_CHECKED : 0);
    UINT win_flags = MF_BYPOSITION | MF_STRING | (g_config.style == CursorStyle::WindowsModern ? MF_CHECKED : 0);
    UINT sys_flags = MF_BYPOSITION | MF_STRING | (g_config.style == CursorStyle::SystemNative ? MF_CHECKED : 0);
    InsertMenuW(hStyleMenu, -1, mac_flags, IDM_STYLE_MAC, L"macOS Classic Pointer (Default)");
    InsertMenuW(hStyleMenu, -1, win_flags, IDM_STYLE_WIN, L"Windows 11 Fluent Pointer");
    InsertMenuW(hStyleMenu, -1, sys_flags, IDM_STYLE_SYSTEM, L"Scale System Pointer");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_POPUP, reinterpret_cast<UINT_PTR>(hStyleMenu), L"Cursor Appearance");

    // Sensitivity Submenu
    HMENU hSensMenu = CreatePopupMenu();
    UINT low_flags = MF_BYPOSITION | MF_STRING | (g_config.sensitivity == ShakeDetector::Sensitivity::Low ? MF_CHECKED : 0);
    UINT med_flags = MF_BYPOSITION | MF_STRING | (g_config.sensitivity == ShakeDetector::Sensitivity::Medium ? MF_CHECKED : 0);
    UINT high_flags = MF_BYPOSITION | MF_STRING | (g_config.sensitivity == ShakeDetector::Sensitivity::High ? MF_CHECKED : 0);
    InsertMenuW(hSensMenu, -1, low_flags, IDM_SENSITIVITY_LOW, L"Low (Requires Vigorous Shake)");
    InsertMenuW(hSensMenu, -1, med_flags, IDM_SENSITIVITY_MED, L"Medium (macOS Balanced Default)");
    InsertMenuW(hSensMenu, -1, high_flags, IDM_SENSITIVITY_HIGH, L"High (Quick Trigger)");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_POPUP, reinterpret_cast<UINT_PTR>(hSensMenu), L"Shake Sensitivity");

    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"Exit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SHAKE_TRIGGERED:
        TriggerShakeAnimation();
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
            ShowContextMenu(hwnd);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_TEST_SHAKE:
            TriggerShakeAnimation();
            break;
        case IDM_AUTOSTART:
            SetAutoStart(!IsAutoStartEnabled());
            break;
        case IDM_PAUSE_FULLSCREEN:
            g_config.pause_in_fullscreen = !g_config.pause_in_fullscreen;
            SaveConfig();
            break;
        case IDM_STYLE_MAC:
            g_config.style = CursorStyle::MacClassic;
            g_renderer.SetStyle(g_config.style);
            SaveConfig();
            TriggerShakeAnimation();
            break;
        case IDM_STYLE_WIN:
            g_config.style = CursorStyle::WindowsModern;
            g_renderer.SetStyle(g_config.style);
            SaveConfig();
            TriggerShakeAnimation();
            break;
        case IDM_STYLE_SYSTEM:
            g_config.style = CursorStyle::SystemNative;
            g_renderer.SetStyle(g_config.style);
            SaveConfig();
            TriggerShakeAnimation();
            break;
        case IDM_SENSITIVITY_LOW:
            g_config.sensitivity = ShakeDetector::Sensitivity::Low;
            g_detector.SetSensitivity(g_config.sensitivity);
            SaveConfig();
            break;
        case IDM_SENSITIVITY_MED:
            g_config.sensitivity = ShakeDetector::Sensitivity::Medium;
            g_detector.SetSensitivity(g_config.sensitivity);
            SaveConfig();
            break;
        case IDM_SENSITIVITY_HIGH:
            g_config.sensitivity = ShakeDetector::Sensitivity::High;
            g_detector.SetSensitivity(g_config.sensitivity);
            SaveConfig();
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;

    case WM_QUERYENDSESSION:
    case WM_ENDSESSION:
        RestoreSystemCursor();
        return TRUE;

    case WM_DESTROY:
        RestoreSystemCursor();
        if (g_nid.hWnd) {
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
        }
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ---------------------------------------------------------------------------
// Installation & Uninstallation Logic
// ---------------------------------------------------------------------------
bool PerformInstall() {
    wchar_t local_app_data[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local_app_data))) {
        MessageBoxW(NULL, L"Could not determine LocalAppData folder.", L"Installation Failed", MB_ICONERROR);
        return false;
    }

    std::wstring target_dir = std::wstring(local_app_data) + L"\\" + APP_NAME;
    CreateDirectoryW(target_dir.c_str(), NULL);
    std::wstring target_exe = target_dir + L"\\" + APP_NAME + L".exe";

    wchar_t current_exe[MAX_PATH];
    GetModuleFileNameW(NULL, current_exe, MAX_PATH);

    if (_wcsicmp(current_exe, target_exe.c_str()) != 0) {
        if (!CopyFileW(current_exe, target_exe.c_str(), FALSE)) {
            MessageBoxW(NULL, L"Failed to copy executable to LocalAppData.", L"Installation Failed", MB_ICONERROR);
            return false;
        }
    }

    // Set registry Run key to point to the installed target executable
    HKEY hkey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0, KEY_SET_VALUE, &hkey) == ERROR_SUCCESS) {
        std::wstring quoted = L"\"" + target_exe + L"\"";
        RegSetValueExW(hkey, APP_NAME, 0, REG_SZ, 
                       reinterpret_cast<const BYTE*>(quoted.c_str()), 
                       static_cast<DWORD>((quoted.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hkey);
    }

    // Launch the installed executable if we were running from a different folder
    if (_wcsicmp(current_exe, target_exe.c_str()) != 0) {
        ShellExecuteW(NULL, L"open", target_exe.c_str(), NULL, target_dir.c_str(), SW_SHOW);
    }

    MessageBoxW(NULL, 
        L"Cursor_Vansh has been successfully installed!\n\n"
        L"- It is running in your System Tray.\n"
        L"- It will start automatically every time Windows boots.\n"
        L"- Shake your mouse rapidly anytime to see your cursor enlarge!", 
        L"Cursor_Vansh Installed", MB_ICONINFORMATION);

    return true;
}

bool PerformUninstall() {
    // 1. Remove Registry Run Key
    HKEY hkey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0, KEY_SET_VALUE, &hkey) == ERROR_SUCCESS) {
        RegDeleteValueW(hkey, APP_NAME);
        RegCloseKey(hkey);
    }

    // 2. Terminate running instance if any
    HWND hwnd = FindWindowW(L"Cursor_VanshHiddenClass", NULL);
    if (hwnd) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }

    MessageBoxW(NULL, 
        L"Cursor_Vansh has been removed from Windows startup.\n"
        L"The background program has been stopped.", 
        L"Cursor_Vansh Uninstalled", MB_ICONINFORMATION);
    return true;
}

bool PerformStatus() {
    bool running = false;
    HWND existing = FindWindowW(L"Cursor_VanshHiddenClass", NULL);
    if (existing) running = true;
    bool autostart = IsAutoStartEnabled();

    std::wstring msg = L"Cursor_Vansh Status:\n\n";
    msg += L"Running: " + std::wstring(running ? L"YES (Active in Background)" : L"NO (Not Running)") + L"\n";
    msg += L"Auto-Start with Windows: " + std::wstring(autostart ? L"YES (Enabled)" : L"NO (Disabled)") + L"\n";

    MessageBoxW(NULL, msg.c_str(), L"Cursor_Vansh Status", MB_ICONINFORMATION);
    return true;
}

// ---------------------------------------------------------------------------
// WinMain Entry Point
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    atexit(RestoreSystemCursor);
    // Parse command line arguments
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (_wcsicmp(argv[i], L"--install") == 0 || _wcsicmp(argv[i], L"-install") == 0 || _wcsicmp(argv[i], L"/install") == 0) {
                bool ok = PerformInstall();
                LocalFree(argv);
                return ok ? 0 : 1;
            }
            if (_wcsicmp(argv[i], L"--uninstall") == 0 || _wcsicmp(argv[i], L"-uninstall") == 0 || _wcsicmp(argv[i], L"/uninstall") == 0) {
                bool ok = PerformUninstall();
                LocalFree(argv);
                return ok ? 0 : 1;
            }
            if (_wcsicmp(argv[i], L"--status") == 0 || _wcsicmp(argv[i], L"-status") == 0 || _wcsicmp(argv[i], L"/status") == 0) {
                PerformStatus();
                LocalFree(argv);
                return 0;
            }
            if (_wcsicmp(argv[i], L"--test") == 0 || _wcsicmp(argv[i], L"-test") == 0 || _wcsicmp(argv[i], L"/test") == 0) {
                HWND existing = FindWindowW(L"Cursor_VanshHiddenClass", NULL);
                if (existing) {
                    PostMessageW(existing, WM_SHAKE_TRIGGERED, 0, 0);
                    LocalFree(argv);
                    return 0;
                }
            }
        }
        LocalFree(argv);
    }

    // Ensure single running instance
    HANDLE hMutex = CreateMutexW(NULL, TRUE, APP_MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Trigger a test shake in existing instance to signal user it's already active
        HWND existing = FindWindowW(L"Cursor_VanshHiddenClass", NULL);
        if (existing) {
            PostMessageW(existing, WM_SHAKE_TRIGGERED, 0, 0);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // High-DPI Awareness
    SetProcessDPIAware();

    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    if (Gdiplus::GdiplusStartup(&g_gdiplus_token, &gdiplusStartupInput, nullptr) != Gdiplus::Ok) {
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    // Load persisted settings
    LoadConfig();

    // Register hidden main window class (receives tray messages)
    WNDCLASSEXW wcMain = { 0 };
    wcMain.cbSize = sizeof(WNDCLASSEXW);
    wcMain.lpfnWndProc = MainWndProc;
    wcMain.hInstance = hInstance;
    wcMain.lpszClassName = L"Cursor_VanshHiddenClass";
    RegisterClassExW(&wcMain);

    g_main_hwnd = CreateWindowExW(0, L"Cursor_VanshHiddenClass", L"Cursor_VanshHidden", 
                                  0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    // Register layered overlay window class
    WNDCLASSEXW wcOverlay = { 0 };
    wcOverlay.cbSize = sizeof(WNDCLASSEXW);
    wcOverlay.lpfnWndProc = OverlayWndProc;
    wcOverlay.hInstance = hInstance;
    wcOverlay.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcOverlay.lpszClassName = L"Cursor_VanshOverlayClass";
    RegisterClassExW(&wcOverlay);

    // Create transparent, click-through, topmost, non-activating overlay window
    g_overlay_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"Cursor_VanshOverlayClass", L"Cursor_VanshOverlay",
        WS_POPUP,
        0, 0, CursorRenderer::BUFFER_SIZE, CursorRenderer::BUFFER_SIZE,
        NULL, NULL, hInstance, NULL);

    // Initialize offscreen DC buffer in renderer
    HDC screen_dc = GetDC(NULL);
    g_renderer.Initialize(screen_dc);
    ReleaseDC(NULL, screen_dc);

    // Install low-level mouse hook
    g_mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);
    if (!g_mouse_hook) {
        MessageBoxW(NULL, L"Failed to install mouse hook.", L"Cursor_Vansh Error", MB_ICONERROR);
        Gdiplus::GdiplusShutdown(g_gdiplus_token);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    // Create tray notification icon
    ZeroMemory(&g_nid, sizeof(NOTIFYICONDATAW));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_main_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    HICON hAppIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    if (!hAppIcon) hAppIcon = LoadIconW(NULL, IDI_APPLICATION);
    g_nid.hIcon = hAppIcon;
    wcscpy_s(g_nid.szTip, L"Cursor_Vansh - Shaking mouse enlarges pointer");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // Main message pump
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup on exit
    RestoreSystemCursor();
    if (g_mouse_hook) {
        UnhookWindowsHookEx(g_mouse_hook);
        g_mouse_hook = nullptr;
    }

    g_renderer.Cleanup();

    if (g_gdiplus_token) {
        Gdiplus::GdiplusShutdown(g_gdiplus_token);
        g_gdiplus_token = 0;
    }

    if (hMutex) {
        CloseHandle(hMutex);
    }

    return 0;
}
