// BatteryTrayApp.cpp
#include <windows.h>
#include <shellapi.h>
#include <tchar.h>
#include <string>
#include <shobjidl.h>  // For SetCurrentProcessExplicitAppUserModelID

// Global config
static int  g_unplugThreshold = 80;   // Unplug threshold
static int  g_plugThreshold = 35;   // Plug threshold
static bool g_noRecurringBelow = false;   // Whether we skip repeated "below threshold" notifications
static int  g_notificationSecs = 30;   // Timeout for notifications, in seconds

// Keep track if we've already shown "below threshold"
static bool g_belowShown = false;

// Unique identifiers
#define WM_TRAYICON       (WM_USER + 1)
#define ID_TRAYICON       1001
#define IDT_BATTERYCHECK  2001

// Global variables
HINSTANCE      g_hInstance;
HWND           g_hWnd;
NOTIFYICONDATA g_nid = { 0 };

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void LoadConfig();
void CheckBattery();
void ShowBalloonTip(const wchar_t* title, const wchar_t* msg, UINT timeoutMs);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    g_hInstance = hInstance;

    // Set an explicit AppUserModelID so Windows sees us as a distinct "app"
    SetCurrentProcessExplicitAppUserModelID(L"BatteryTrayApp.Lite.1.0");

    // Load thresholds and other configs
    LoadConfig();

    // Register window class
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BatteryTrayAppClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    // Create hidden window
    g_hWnd = CreateWindow(L"BatteryTrayAppClass",
        L"Battery Tray App",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        200, 200,
        NULL, NULL, hInstance, NULL);

    // Setup NOTIFYICONDATA
    ZeroMemory(&g_nid, sizeof(NOTIFYICONDATA));
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = g_hWnd;
    g_nid.uID = ID_TRAYICON;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_INFORMATION);
    wcscpy_s(g_nid.szTip, L"Battery Monitor");

    // Add icon to tray
    Shell_NotifyIcon(NIM_ADD, &g_nid);

    // Use the latest tray icon features on Windows 10/11
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIcon(NIM_SETVERSION, &g_nid);

    // Show initial balloon
    ShowBalloonTip(
        L"Battery Monitor",
        L"Battery Monitor is running.\nWe'll notify you about connecting/disconnecting the charger.",
        g_notificationSecs * 1000
    );

    // Timer for battery checks every 30 seconds
    SetTimer(g_hWnd, IDT_BATTERYCHECK, 30000, NULL);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup tray icon
    Shell_NotifyIcon(NIM_DELETE, &g_nid);

    return (int)msg.wParam;
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_TRAYICON:
    {
        if (LOWORD(lParam) == WM_RBUTTONUP)
        {
            // Right-click: show a small menu with "Exit"
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, 1, L"Exit");

            // Show the menu at the cursor
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);

            int cmd = TrackPopupMenu(
                hMenu,
                TPM_RETURNCMD | TPM_NONOTIFY,
                pt.x, pt.y,
                0, hWnd, NULL
            );

            if (cmd == 1) {
                PostQuitMessage(0);
            }
            DestroyMenu(hMenu);
        }
        break;
    }
    case WM_TIMER:
        if (wParam == IDT_BATTERYCHECK)
        {
            CheckBattery();
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Load config from .ini
void LoadConfig()
{
    wchar_t iniPath[MAX_PATH];
    GetModuleFileName(NULL, iniPath, MAX_PATH);

    // Replace .exe with .ini
    wchar_t* dot = wcsrchr(iniPath, L'.');
    if (dot) wcscpy_s(dot, MAX_PATH - (dot - iniPath), L".ini");

    // Read thresholds
    g_unplugThreshold = GetPrivateProfileInt(L"Settings", L"UnplugThreshold", 80, iniPath);
    g_plugThreshold = GetPrivateProfileInt(L"Settings", L"PlugThreshold", 35, iniPath);

    // Read "NoRecurringBelow" as "true" or "false"
    wchar_t buf[16] = { 0 };
    GetPrivateProfileString(L"Settings", L"NoRecurringBelow", L"false", buf, 16, iniPath);
    _wcslwr_s(buf);
    g_noRecurringBelow = (wcscmp(buf, L"true") == 0);

    // Read NotificationTimeout (in seconds)
    int defaultTimeout = 30;
    int readValue = GetPrivateProfileInt(L"Settings", L"NotificationTimeout", defaultTimeout, iniPath);
    // If user sets negative or zero, let's clamp it to at least 5 seconds
    if (readValue < 5) readValue = 5;
    g_notificationSecs = readValue;
}

// Check battery
void CheckBattery()
{
    // Reload config each time so changes take effect immediately
    LoadConfig();

    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps))
    {
        int  percent = sps.BatteryLifePercent;
        bool onAC = (sps.ACLineStatus == 1);

        if (onAC)
        {
            // Reset if user is on AC
            g_belowShown = false;

            if (percent >= g_unplugThreshold)
            {
                ShowBalloonTip(
                    L"Battery Monitor",
                    L"Battery above threshold. You can unplug now.",
                    g_notificationSecs * 1000
                );
            }
        }
        else
        {
            // On battery
            if (percent <= g_plugThreshold)
            {
                // Only show "below threshold" if not recurring or not shown yet
                if (!g_noRecurringBelow || !g_belowShown)
                {
                    ShowBalloonTip(
                        L"Battery Monitor",
                        L"Battery below threshold. Please plug in.",
                        g_notificationSecs * 1000
                    );
                    g_belowShown = true;
                }
            }
        }
    }
}

// Show balloon tip with requested timeout
void ShowBalloonTip(const wchar_t* title, const wchar_t* msg, UINT timeoutMs)
{
    NOTIFYICONDATA balloon = g_nid;
    balloon.uFlags = NIF_INFO | NIF_SHOWTIP;
    balloon.dwInfoFlags = NIIF_INFO;
    balloon.uTimeout = timeoutMs; // Windows may clamp or ignore
    wcscpy_s(balloon.szInfoTitle, title);
    wcscpy_s(balloon.szInfo, msg);

    Shell_NotifyIcon(NIM_MODIFY, &balloon);
}
