#include <windows.h>
#include <shellapi.h>
#include <tchar.h>
#include <string>
#include <shobjidl.h>  // For SetCurrentProcessExplicitAppUserModelID
#include <sstream>

// ---------------------------------------------------------------------------
// Global config
static int  g_unplugThreshold = 80;   // Disconnect threshold
static int  g_plugThreshold = 35;   // Connect threshold
static bool g_noRecurringBelow = false; // Skip repeated "connect" notifications
static bool g_noRecurringAbove = false; // Skip repeated "disconnect" notifications
static int  g_notificationSecs = 30;   // Timeout for notifications, in seconds

// Notification interval config
static std::wstring g_intervalType = L"time";  // "time" or "percentage"
static int          g_intervalValue = 5;       // number of minutes or % points

// State tracking
static bool  g_belowShown = false; // have we shown "connect" recently?
static bool  g_aboveShown = false; // have we shown "disconnect" recently?

// For limiting notification intervals
static DWORD g_lastNotifyTime = 0; // GetTickCount() of last notification
static int   g_lastNotifyBattery = -1;

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
bool CanShowNotification(int currentBattery);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    g_hInstance = hInstance;

    // Set an explicit AppUserModelID so Windows sees us as a distinct "app"
    SetCurrentProcessExplicitAppUserModelID(L"BatteryNotifier.Lite.1.0");

    // Load thresholds and other configs
    LoadConfig();

    // Register window class
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BatteryNotifierClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    // Create hidden window
    g_hWnd = CreateWindow(
        L"BatteryNotifierClass",
        L"BatteryNotifier",
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
    wcscpy_s(g_nid.szTip, L"BatteryNotifier");

    // Add icon to tray
    Shell_NotifyIcon(NIM_ADD, &g_nid);

    // Use the latest tray icon features on Windows 10/11
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIcon(NIM_SETVERSION, &g_nid);

    // Show initial balloon with user thresholds
    {
        std::wstringstream ss;
        ss << L"BatteryNotifier running.\n"
            << L"Connect at " << g_plugThreshold << L"%.\n"
            << L"Disconnect at " << g_unplugThreshold << L"%.";
        ShowBalloonTip(
            L"BatteryNotifier",
            ss.str().c_str(),
            g_notificationSecs * 1000
        );
    }

    // Set a timer to check battery every 30 seconds (you can adjust this)
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

    // Read "NoRecurringBelow"
    {
        wchar_t buf[16] = { 0 };
        GetPrivateProfileString(L"Settings", L"NoRecurringBelow", L"false", buf, 16, iniPath);
        _wcslwr_s(buf);
        g_noRecurringBelow = (wcscmp(buf, L"true") == 0);
    }

    // Read "NoRecurringAbove"
    {
        wchar_t buf[16] = { 0 };
        GetPrivateProfileString(L"Settings", L"NoRecurringAbove", L"false", buf, 16, iniPath);
        _wcslwr_s(buf);
        g_noRecurringAbove = (wcscmp(buf, L"true") == 0);
    }

    // Read NotificationTimeout (in seconds)
    {
        int defaultTimeout = 30;
        int readValue = GetPrivateProfileInt(L"Settings", L"NotificationTimeout", defaultTimeout, iniPath);
        if (readValue < 5) readValue = 5;
        g_notificationSecs = readValue;
    }

    // Read NotificationIntervalType ("time" or "percentage")
    {
        wchar_t buf[32] = { 0 };
        GetPrivateProfileString(L"Settings", L"NotificationIntervalType", L"time", buf, 32, iniPath);
        // Convert to lowercase
        _wcslwr_s(buf);
        g_intervalType = buf;
        if (g_intervalType != L"time" && g_intervalType != L"percentage")
        {
            // default to time if invalid
            g_intervalType = L"time";
        }
    }

    // Read NotificationIntervalValue
    {
        int val = GetPrivateProfileInt(L"Settings", L"NotificationIntervalValue", 5, iniPath);
        if (val < 1) val = 1; // clamp at 1
        g_intervalValue = val;
    }
}

// Decide whether we can show a new notification based on time or percentage
bool CanShowNotification(int currentBattery)
{
    DWORD now = GetTickCount();

    if (g_intervalType == L"time")
    {
        // Check how many ms have passed since last notification
        DWORD elapsedMs = now - g_lastNotifyTime;
        // Convert user-specified minutes to milliseconds
        DWORD neededMs = (DWORD)g_intervalValue * 60 * 1000;
        if (elapsedMs < neededMs)
            return false;
    }
    else
    {
        // "percentage" mode
        // Check the difference in battery level from last notification
        if (g_lastNotifyBattery >= 0)
        {
            int diff = abs(currentBattery - g_lastNotifyBattery);
            if (diff < g_intervalValue)
                return false;
        }
    }

    // If we get here, it's okay to show
    g_lastNotifyTime = now;
    g_lastNotifyBattery = currentBattery;
    return true;
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

        // If enough time/percentage hasn't passed, skip checking notifications entirely
        // so we don't spam the user.
        if (!CanShowNotification(percent)) {
            return;
        }

        if (onAC)
        {
            // Reset the "connect" shown state since user is on AC
            g_belowShown = false;

            if (percent >= g_unplugThreshold)
            {
                // Only show if not recurring or not yet shown
                if (!g_noRecurringAbove || !g_aboveShown)
                {
                    std::wstringstream ss;
                    ss << L"Battery is at " << percent << L"%. "
                        << L"You can disconnect your charger.";
                    ShowBalloonTip(
                        L"BatteryNotifier",
                        ss.str().c_str(),
                        g_notificationSecs * 1000
                    );
                    g_aboveShown = true;
                }
            }
            else
            {
                // If not above threshold, reset the "above" shown state
                g_aboveShown = false;
            }
        }
        else
        {
            // Reset the "disconnect" shown state since user is on battery
            g_aboveShown = false;

            if (percent <= g_plugThreshold)
            {
                // Only show "connect" if not recurring or not shown yet
                if (!g_noRecurringBelow || !g_belowShown)
                {
                    std::wstringstream ss;
                    ss << L"Battery is at " << percent << L"%. "
                        << L"Please connect your charger.";
                    ShowBalloonTip(
                        L"BatteryNotifier",
                        ss.str().c_str(),
                        g_notificationSecs * 1000
                    );
                    g_belowShown = true;
                }
            }
            else
            {
                // If not below threshold, reset the "below" shown state
                g_belowShown = false;
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
