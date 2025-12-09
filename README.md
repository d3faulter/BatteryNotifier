# BatteryNotifier

The most lightweight Windows tray application for charging notifications. It notifies you when your laptop battery reaches user-defined thresholds for connecting or disconnecting the charger to prolong battery life. Uses **under 1 MB of RAM** in most cases!

**Purpose**: Even though many laptops have built in support for charge tresholds, many modern laptops (e.g. most non-business HP laptops) lack the hardware support for it. The only way to circumvent it is by manually stopping the charging, which is often difficult to remember. Thus, a tiny reminder app can do it for you. 

![image](https://github.com/user-attachments/assets/ba511a98-06e0-4d29-bdad-51c21d8e5a04)
![image](https://github.com/user-attachments/assets/260ce7bb-e010-438d-a29d-8e2d7e63de3a)



## Features

- **Tiny footprint**: Written in C++ with raw Win32 APIs, typically under **1 MB** memory use.
- **Simple config**: Set thresholds in an `.ini` file. The app reloads it on-the-fly every 30 seconds.
- **Balloon notifications**: Displays balloon tips (legacy Windows style) by default, which may appear in Action Center on Windows 10/11.
- **Enhanced notification control**: Notifications are now configurable based on time intervals or percentage changes.
- **User-configured thresholds display**: Initial balloon tip shows user-configured charging thresholds for quick reference.

![image](https://github.com/user-attachments/assets/a509f4e8-bf50-4aad-b171-8dc526d52762)

## Configuration

Create or edit a `.ini` file named `BatteryNotifier.ini` in the same folder as the `.exe`. Example:  



```ini
[Settings]
UnplugThreshold=80
PlugThreshold=35
NoRecurringBelow=true
NotificationTimeout=30
NotificationIntervalType=time
NotificationIntervalValue=5
```
**Note:** Please restart application after making changes (right-click tray icon).  

### Explanation:
- **UnplugThreshold**: If battery >= this value and you’re on AC, you’ll see an "unplug" balloon.
- **PlugThreshold**: If battery <= this value and you’re on battery, you’ll see a "plug in" balloon.
- **NoRecurringBelow**: `true` or `false`. If `true`, only one "below threshold" balloon until you plug in again.
- **NotificationTimeout**: Time (in seconds) to request the balloon stay on screen (Windows may ignore or clamp it).
- **NotificationIntervalType**: `time` or `percentage`. Determines how notification frequency is calculated.
  - `time`: Notifications are shown at intervals defined by `NotificationIntervalValue` (in minutes).
  - `percentage`: Notifications are shown only if the battery percentage changes by the specified amount.
- **NotificationIntervalValue**: Time in minutes (if `NotificationIntervalType` is `time`) or percentage points.

## Setup & Usage

Either download the compiled x64 version from releases or follow the steps below. Keep `.exe` (app) and `.ini` (config file) in the same folder.

1. Compile the application in Visual Studio (with the "Desktop development with C++" workload) or using the MSVC command line.
2. Place `BatteryNotifier.exe` and `BatteryNotifier.ini` in the same folder.
4. Run the `.exe`. A tray icon will appear (hover text: "BatteryNotifier").
5. That's it. Right-click the tray icon and select "Exit" to quit the application.

**Applying config changes**: To apply changes in the config, just restart the app.

## Improvements in the Latest Version

- **Refactored code**: Improved readability and maintainability by organizing configuration loading and state tracking.
- **New configuration options**: Added `NotificationIntervalType` and `NotificationIntervalValue` for greater control over notification intervals.
- **Notification management**: Introduced the `CanShowNotification` function to control notification frequency.
- **Enhanced initial balloon tip**: Displays user-configured thresholds for quick reference.
- **Updated logic**: Modified `CheckBattery` to reset notification states and integrate the `CanShowNotification` function.
- **State variables**: Added `g_aboveShown` and `g_lastNotifyBattery` for advanced notification tracking.
- **Improved messages**: Notifications are now more descriptive and informative.
- **Graceful handling**: Ensures application stability with invalid or missing configuration values.

## Notes

- Windows 10/11 might show balloon tips briefly or in the Action Center, ignoring long timeouts.
- Notifications may not always appear in the Notification Center or Action Center due to how Windows handles legacy balloon notifications.
- For truly persistent notifications requiring user dismissal, you would need modern Windows Toast notifications (more resource heavy).

## To-Do

- **Check for duplicate processes on start**: Currently multiple processes will start when opening the app with existing process running (can currently be closed manually by right-clicking tray icon or task manager)
