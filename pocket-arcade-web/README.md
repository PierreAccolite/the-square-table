# Pocket Arcade Web USB Test

Small browser-only test for the ESP32 USB game platform.

## ESP32 sketch

Use:

projects/esp32_usb_game_platform.ino

The ESP32 uses 115200 baud, 8-N-1.

## Test

Open index.html from a browser that supports Web Serial.

Press CONNECT ESP32 and select the ESP32 USB serial device.

The page sends PING automatically and should receive:

PONG

Then press START STREAM. The joystick values should update on the page.

This is intentionally a tiny hardware test. The full arcade UI will be built only after USB browser communication is proven.

## Important

Web Serial is browser/security dependent. If the browser does not expose navigator.serial, this test will report that clearly rather than pretending the connection works.
