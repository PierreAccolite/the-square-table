# Pocket Arcade Android USB Host

First Android host prototype for the Pocket Arcade USB Game Platform.

## What it does

- Runs as a landscape/full-screen Android app
- Detects a USB serial ESP32 through Android USB Host / OTG
- Uses 115200 8-N-1
- Requests Android USB permission
- Opens the serial port automatically
- Sends `PING`, `STATUS`, and `INPUT`
- Displays live joystick X/Y/button values
- Provides START / SELECT / MENU / BACK controls
- Keeps the ESP32 diagnostic/platform sketch unchanged

## ESP32 side

Use:

`projects/esp32_usb_game_platform.ino`

The Android app expects the existing text protocol:

- `READY,...`
- `PONG`
- `STATUS,...`
- `JOY,x,y,button`
- `EVENT,...`
- `HEARTBEAT,...`

## Build

Open the `pocket-arcade-android` folder in Android Studio.

Android Studio can import the Gradle project and download the Android/Gradle dependencies.

The USB serial dependency is:

`com.github.mik3y:usb-serial-for-android:3.11.0`

The library supports Android USB Host mode and common USB serial bridges including FTDI, CP210x, CH340/CH341 and Prolific.

## First field test

1. Flash `esp32_usb_game_platform.ino` to the ESP32.
2. Confirm the ESP32 prints `READY,POCKET_ARCADE_USB`.
3. Connect ESP32 USB to the Android phone/radio using USB OTG/host mode.
4. Start Pocket Arcade.
5. Approve the USB permission prompt.
6. The screen should show `USB: CONNECTED`.
7. Move the joystick. X/Y values should update live.
8. Press START/SELECT/MENU/BACK and watch the ESP32 serial monitor for the corresponding `EVENT`.

## Next stage

This is deliberately only the host/control layer.

Once USB is proven inside the app, the next stage is the actual big-screen game renderer and game-selection UI.