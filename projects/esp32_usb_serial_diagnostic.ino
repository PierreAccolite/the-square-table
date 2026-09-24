/*
  POCKET ARCADE - USB SERIAL DIAGNOSTIC
  -------------------------------------
  Classic ESP32 Dev Module / ESP32-D0WDQ6

  Purpose:
    Test the physical USB -> USB/UART bridge -> ESP32 serial path
    without touching the main Pocket Arcade sketch.

  IMPORTANT:
    This is NOT native USB on the ESP32-D0WDQ6.
    The development board's USB-to-UART bridge is used.

  Serial:
    115200 baud

  Commands:
    PING       -> PONG
    STATUS     -> basic board status
    JOY        -> joystick ADC values
    HELP       -> command list
    TEST       -> short automated telemetry test

  The sketch also prints a heartbeat every 2 seconds.

  This file is intentionally standalone so it remains useful as a
  permanent hardware test tool.
*/

#include <Arduino.h>

static const int JOY_X_PIN = 32;
static const int JOY_Y_PIN = 33;
static const int JOY_BTN_PIN = 25;

unsigned long lastHeartbeat = 0;
unsigned long bootTime = 0;

void printHeader() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("     POCKET ARCADE USB SERIAL TEST");
  Serial.println("========================================");
  Serial.println("ESP32 -> USB serial link test");
  Serial.println("Baud: 115200");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  PING   - reply with PONG");
  Serial.println("  STATUS - board and serial status");
  Serial.println("  JOY    - joystick readings");
  Serial.println("  HELP   - show commands");
  Serial.println("  TEST   - automated USB telemetry test");
  Serial.println();
  Serial.println("READY");
  Serial.println("----------------------------------------");
}

void printStatus() {
  Serial.println();
  Serial.println("[STATUS]");
  Serial.print("Chip: ");
  Serial.println(ESP.getChipModel());

  Serial.print("CPU: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");

  Serial.print("Flash: ");
  Serial.print(ESP.getFlashChipSize() / 1024UL / 1024UL);
  Serial.println(" MB");

  Serial.print("Heap free: ");
  Serial.println(ESP.getFreeHeap());

  Serial.print("Uptime: ");
  Serial.print((millis() - bootTime) / 1000UL);
  Serial.println(" seconds");

  Serial.print("USB serial baud: ");
  Serial.println(115200);

  Serial.println();
}

void printJoystick() {
  int x = analogRead(JOY_X_PIN);
  int y = analogRead(JOY_Y_PIN);
  int btn = digitalRead(JOY_BTN_PIN);

  Serial.print("[JOY] X=");
  Serial.print(x);
  Serial.print(" Y=");
  Serial.print(y);
  Serial.print(" BTN=");
  Serial.println(btn == LOW ? "PRESSED" : "RELEASED");
}

void runTest() {
  Serial.println();
  Serial.println("[TEST] USB telemetry test starting...");
  Serial.println("[TEST] If you can read this, ESP32 -> USB works.");

  for (int i = 1; i <= 5; i++) {
    Serial.print("[TEST] Packet ");
    Serial.print(i);
    Serial.print(" | millis=");
    Serial.print(millis());
    Serial.print(" | X=");
    Serial.print(analogRead(JOY_X_PIN));
    Serial.print(" | Y=");
    Serial.print(analogRead(JOY_Y_PIN));
    Serial.print(" | BTN=");
    Serial.println(digitalRead(JOY_BTN_PIN) == LOW ? "1" : "0");
    delay(500);
  }

  Serial.println("[TEST] Complete.");
  Serial.println();
}

void handleCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command.length() == 0) {
    return;
  }

  Serial.print("> ");
  Serial.println(command);

  if (command == "PING") {
    Serial.println("PONG");
  }
  else if (command == "STATUS") {
    printStatus();
  }
  else if (command == "JOY") {
    printJoystick();
  }
  else if (command == "HELP") {
    printHeader();
  }
  else if (command == "TEST") {
    runTest();
  }
  else {
    Serial.print("Unknown command: ");
    Serial.println(command);
    Serial.println("Type HELP for commands.");
  }
}

void setup() {
  pinMode(JOY_BTN_PIN, INPUT_PULLUP);

  analogReadResolution(12);

  Serial.begin(115200);
  delay(500);

  bootTime = millis();

  printHeader();
}

void loop() {
  // Receive commands from the USB serial connection.
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    handleCommand(command);
  }

  // Simple heartbeat so a connected host can see that the ESP32
  // remains alive even when no commands are being sent.
  if (millis() - lastHeartbeat >= 2000) {
    lastHeartbeat = millis();

    Serial.print("[HEARTBEAT] uptime=");
    Serial.print((millis() - bootTime) / 1000UL);
    Serial.print("s heap=");
    Serial.println(ESP.getFreeHeap());
  }

  delay(5);
}
