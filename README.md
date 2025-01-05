# BatteryNotifier

The most lightweight Windows tray application for charging notifications. It notifies you when your laptop battery reaches user-defined thresholds for connecting or disconnecting the charger to prolong battery life. Uses **under 1 MB of RAM** in most cases!

![image](https://github.com/user-attachments/assets/ba511a98-06e0-4d29-bdad-51c21d8e5a04)

## Features

- **Tiny footprint**: Written in C++ with raw Win32 APIs, typically under **1 MB** memory use.
- **Simple config**: Set thresholds in an `.ini` file. The app reloads it on-the-fly every 30 seconds.
- **Balloon notifications**: Displays balloon tips (legacy Windows style) by default, which may appear in Action Center on Windows 10/11.

![image](https://github.com/user-attachments/assets/a509f4e8-bf50-4aad-b171-8dc526d52762)


## Configuration

Create or edit a `.ini` file named `BatteryNotifier.ini` in the same folder as the `.exe`. Example:

```ini
[Settings]
UnplugThreshold=80
PlugThreshold=35
NoRecurringBelow=true
NotificationTimeout=30
```

### Explanation:
- **UnplugThreshold**: If battery >= this value and you’re on AC, you’ll see an "unplug" balloon.
- **PlugThreshold**: If battery <= this value and you’re on battery, you’ll see a "plug in" balloon.
- **NoRecurringBelow**: `true` or `false`. If `true`, only one "below threshold" balloon until you plug in again.
- **NotificationTimeout**: Time (in seconds) to request the balloon stay on screen (Windows may ignore or clamp it).

## Setup & Usage

Either download the compiled x64 version from releases or follow below steps. Keep ".exe" (app) and ".ini" (config file) in the same folder. 

1. Compile the application in Visual Studio (with the "Desktop development with C++" workload) or using the MSVC command line.
2. Place `BatteryNotifier.exe` and `BatteryNotifier.ini` in the same folder.
3. Run the `.exe`. A tray icon will appear (hover text: "Battery Monitor").
4. Right-click the tray icon and select "Exit" to quit the application.

## Notes

- Windows 10/11 might show balloon tips briefly or in the Action Center, ignoring long timeouts.
- Notifications may not always appear in the Notification Center or Action Center due to how Windows handles legacy balloon notifications.
- For truly persistent notifications requiring user dismissal, you would need modern Windows Toast notifications (more resource heavy).
