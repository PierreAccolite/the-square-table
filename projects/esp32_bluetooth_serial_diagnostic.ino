/*
  POCKET ARCADE - BLUETOOTH CLASSIC SERIAL DIAGNOSTIC
  ----------------------------------------------------
  Hardware:
    Classic ESP32 / ESP32-D0WDQ6

  Purpose:
    Standalone Bluetooth Classic SPP test.

  This sketch deliberately does NOT modify the main Pocket Arcade
  sketch or the USB diagnostic. It is a permanent hardware test.

  Android:
    Pair/connect to the Bluetooth device named:
      POCKET_ARCADE_BT

    Use an Android Bluetooth Classic SPP/serial terminal application.

  Commands:
    PING       -> PONG
    STATUS     -> board status
    JOY        -> joystick readings
    STREAM ON  -> continuous joystick telemetry
    STREAM OFF -> stop telemetry
    HELP       -> command list

  Expected data path:

      Android
         ⇅ Bluetooth Classic SPP
      ESP32-D0WDQ6
         ├── GPIO32 joystick X
         ├── GPIO33 joystick Y
         └── GPIO25 joystick button

  Serial monitor:
    USB serial remains available at 115200 for diagnostics.
*/

#include <Arduino.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

static const char* BT_DEVICE_NAME = "POCKET_ARCADE_BT";

static const int JOY_X_PIN = 32;
static const int JOY_Y_PIN = 33;
static const int JOY_BTN_PIN = 25;

unsigned long bootTime = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastStream = 0;

bool streamEnabled = false;

void printUSB(const String& message) {
  Serial.println(message);
}

void printBT(const String& message) {
  if (SerialBT.hasClient()) {
    SerialBT.println(message);
  }
}

void printBoth(const String& message) {
  printUSB(message);
  printBT(message);
}

void printHelp() {
  printBoth("");
  printBoth("========================================");
  printBoth("     POCKET ARCADE BLUETOOTH TEST");
  printBoth("========================================");
  printBoth("Commands:");
  printBoth("  PING       - reply with PONG");
  printBoth("  STATUS     - Bluetooth/board status");
  printBoth("  JOY        - joystick readings");
  printBoth("  STREAM ON  - continuous joystick data");
  printBoth("  STREAM OFF - stop joystick data");
  printBoth("  HELP       - show commands");
  printBoth("");
}

void printJoystick() {
  int x = analogRead(JOY_X_PIN);
  int y = analogRead(JOY_Y_PIN);
  int btn = digitalRead(JOY_BTN_PIN);

  String line = "[JOY] X=" + String(x) +
                " Y=" + String(y) +
                " BTN=" + (btn == LOW ? "PRESSED" : "RELEASED");

  printBoth(line);
}

void printStatus() {
  printBoth("");
  printBoth("[STATUS]");

  printBoth("Device: " + String(BT_DEVICE_NAME));

  printBoth("Bluetooth client: " +
            String(SerialBT.hasClient() ? "CONNECTED" : "NOT CONNECTED"));

  printBoth("Chip: " + String(ESP.getChipModel()));

  printBoth("CPU: " + String(ESP.getCpuFreqMHz()) + " MHz");

  printBoth("Flash: " +
            String(ESP.getFlashChipSize() / 1024UL / 1024UL) +
            " MB");

  printBoth("Heap free: " + String(ESP.getFreeHeap()));

  printBoth("Uptime: " +
            String((millis() - bootTime) / 1000UL) +
            " seconds");

  printBoth("Stream: " + String(streamEnabled ? "ON" : "OFF"));

  printBoth("");
}

void handleCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command.length() == 0) {
    return;
  }

  printBoth("> " + command);

  if (command == "PING") {
    printBoth("PONG");
  }
  else if (command == "STATUS") {
    printStatus();
  }
  else if (command == "JOY") {
    printJoystick();
  }
  else if (command == "STREAM ON") {
    streamEnabled = true;
    printBoth("[STREAM] ON");
  }
  else if (command == "STREAM OFF") {
    streamEnabled = false;
    printBoth("[STREAM] OFF");
  }
  else if (command == "HELP") {
    printHelp();
  }
  else {
    printBoth("Unknown command. Type HELP.");
  }
}

void setup() {
  pinMode(JOY_BTN_PIN, INPUT_PULLUP);
  analogReadResolution(12);

  Serial.begin(115200);
  delay(500);

  bootTime = millis();

  printUSB("");
  printUSB("========================================");
  printUSB("   POCKET ARCADE BLUETOOTH SERIAL TEST");
  printUSB("========================================");

  // true = master mode enabled as required by BluetoothSerial/SPP.
  if (!SerialBT.begin(BT_DEVICE_NAME)) {
    printUSB("[BT] START FAILED");
  }
  else {
    printUSB("[BT] STARTED");
    printUSB("[BT] Device name: " + String(BT_DEVICE_NAME));
    printUSB("[BT] Waiting for Android Bluetooth SPP client...");
  }

  printUSB("[BT] Classic Bluetooth SPP");
  printUSB("[BT] USB serial remains active at 115200");
  printUSB("");
  printUSB("READY");
  printUSB("----------------------------------------");
}

void loop() {
  // USB serial command input.
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    handleCommand(command);
  }

  // Bluetooth command input.
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    handleCommand(command);
  }

  // Continuous joystick telemetry over Bluetooth.
  if (streamEnabled && SerialBT.hasClient() &&
      millis() - lastStream >= 100) {

    lastStream = millis();

    int x = analogRead(JOY_X_PIN);
    int y = analogRead(JOY_Y_PIN);
    int btn = digitalRead(JOY_BTN_PIN);

    SerialBT.print("JOY,");
    SerialBT.print(x);
    SerialBT.print(",");
    SerialBT.print(y);
    SerialBT.print(",");
    SerialBT.println(btn == LOW ? 1 : 0);
  }

  // USB-only heartbeat.
  if (millis() - lastHeartbeat >= 2000) {
    lastHeartbeat = millis();

    Serial.print("[HEARTBEAT] uptime=");
    Serial.print((millis() - bootTime) / 1000UL);
    Serial.print("s heap=");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" BT=");
    Serial.println(SerialBT.hasClient() ? "CONNECTED" : "WAITING");
  }

  delay(5);
}
