/*
  POCKET ARCADE - USB GAME PLATFORM
  ----------------------------------
  Classic ESP32-D0WDQ6

  PURPOSE
  -------
  This is the new USB-based platform sketch for the Pocket Arcade.

  Preserved diagnostics:
    projects/esp32_usb_serial_diagnostic.ino
    projects/esp32_bluetooth_serial_diagnostic.ino
    projects/esp32_ble_scan_diagnostic.ino
    projects/esp32_wifi_diagnostic.ino
    projects/esp32_hardware_inventory.ino

  Main arcade code remains separate:
    projects/esp32_dev_arcade.ino

  CURRENT ARCHITECTURE
  --------------------
      Android phone / Android radio
                  |
                 USB
                  |
          USB-to-UART bridge
                  |
                ESP32
              /       \
         Joystick      OLED
          GPIO32/33    I2C

  USB protocol
  ------------
  Text lines are used initially because they are easy to debug.

  Android -> ESP32:
    PING
    STATUS
    JOY
    INPUT
    START
    MENU
    SELECT
    BACK

  ESP32 -> Android:
    READY
    PONG
    STATUS,...
    JOY,...
    INPUT,...
    EVENT,...

  This is deliberately a clean platform foundation. Actual game
  rendering/protocol can be added without modifying the USB diagnostic.
*/

#include <Arduino.h>

static const int JOY_X_PIN = 32;
static const int JOY_Y_PIN = 33;
static const int JOY_BTN_PIN = 25;

static const uint32_t USB_BAUD = 74880;
static const uint32_t HEARTBEAT_MS = 2000;
static const uint32_t INPUT_REPORT_MS = 50;

// Joystick calibration based on the observed hardware.
// These are deliberately broad defaults and can be refined later.
static const int JOY_X_MIN = 0;
static const int JOY_X_MAX = 4095;
static const int JOY_Y_MIN = 0;
static const int JOY_Y_MAX = 4095;

// Current joystick centre measured from the physical controller.
// These can be refined after live testing.
static const int JOY_X_CENTER = 2870;
static const int JOY_Y_CENTER = 2780;
static const int JOY_DEADZONE = 8;

unsigned long bootTime = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastInputReport = 0;

bool inputStreaming = false;

struct ControllerState {
  int rawX;
  int rawY;
  bool button;
};

ControllerState controller = {0, 0, false};

int clampInt(int value, int minimum, int maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

int normalizeAxis(int value, int minimum, int maximum, int center) {
  value = clampInt(value, minimum, maximum);

  long result;

  // Map each side independently around the physical centre so that
  // the joystick rests at approximately 0 instead of +40/+36.
  if (value >= center) {
    result = map(value, center, maximum, 0, 100);
  } else {
    result = map(value, minimum, center, -100, 0);
  }

  // Apply a small dead zone around centre to prevent ADC noise/drift.
  if (abs((int)result) <= JOY_DEADZONE) {
    return 0;
  }

  return clampInt((int)result, -100, 100);
}

void readController() {
  controller.rawX = analogRead(JOY_X_PIN);
  controller.rawY = analogRead(JOY_Y_PIN);
  controller.button = digitalRead(JOY_BTN_PIN) == LOW;
}

void sendReady() {
  Serial.println("READY,POCKET_ARCADE_USB");
}

void sendPong() {
  Serial.println("PONG");
}

void sendStatus() {
  readController();

  Serial.print("STATUS,");
  Serial.print("chip=");
  Serial.print(ESP.getChipModel());

  Serial.print(",cpu=");
  Serial.print(ESP.getCpuFreqMHz());

  Serial.print(",flash_mb=");
  Serial.print(ESP.getFlashChipSize() / 1024UL / 1024UL);

  Serial.print(",heap=");
  Serial.print(ESP.getFreeHeap());

  Serial.print(",uptime=");
  Serial.print((millis() - bootTime) / 1000UL);

  Serial.print(",stream=");
  Serial.println(inputStreaming ? "1" : "0");
}

void sendJoystick() {
  readController();

  int x = normalizeAxis(controller.rawX, JOY_X_MIN, JOY_X_MAX, JOY_X_CENTER);
  int y = normalizeAxis(controller.rawY, JOY_Y_MIN, JOY_Y_MAX, JOY_Y_CENTER);

  Serial.print("JOY,");
  Serial.print(x);
  Serial.print(",");
  Serial.print(y);
  Serial.print(",");
  Serial.println(controller.button ? "1" : "0");
}

void sendRawJoystick() {
  readController();

  Serial.print("JOYRAW,");
  Serial.print(controller.rawX);
  Serial.print(",");
  Serial.print(controller.rawY);
  Serial.print(",");
  Serial.println(controller.button ? "1" : "0");
}

void sendInputEvent(const char* eventName) {
  Serial.print("EVENT,");
  Serial.println(eventName);
}

void handleCommand(String command) {
  command.trim();

  if (command.length() == 0) {
    return;
  }

  String upper = command;
  upper.toUpperCase();

  if (upper == "PING") {
    sendPong();
  }
  else if (upper == "STATUS") {
    sendStatus();
  }
  else if (upper == "JOY") {
    sendJoystick();
  }
  else if (upper == "JOYRAW") {
    sendRawJoystick();
  }
  else if (upper == "INPUT") {
    inputStreaming = true;
    Serial.println("INPUT,STREAM,ON");
  }
  else if (upper == "INPUT OFF") {
    inputStreaming = false;
    Serial.println("INPUT,STREAM,OFF");
  }
  else if (upper == "START") {
    sendInputEvent("START");
  }
  else if (upper == "SELECT") {
    sendInputEvent("SELECT");
  }
  else if (upper == "MENU") {
    sendInputEvent("MENU");
  }
  else if (upper == "BACK") {
    sendInputEvent("BACK");
  }
  else if (upper == "HELP") {
    Serial.println("HELP,PING|STATUS|JOY|JOYRAW|INPUT|INPUT OFF|START|SELECT|MENU|BACK|HELP");
  }
  else {
    Serial.print("ERROR,UNKNOWN_COMMAND,");
    Serial.println(command);
  }
}

void setup() {
  pinMode(JOY_BTN_PIN, INPUT_PULLUP);
  analogReadResolution(12);

  Serial.begin(USB_BAUD);
  delay(500);

  bootTime = millis();

  Serial.println();
  Serial.println("========================================");
  Serial.println("       POCKET ARCADE USB PLATFORM");
  Serial.println("========================================");
  Serial.println("Transport: USB Serial");
  Serial.print("Baud: ");
  Serial.println(USB_BAUD);
  Serial.println("Controller: GPIO32 / GPIO33 / GPIO25");
  Serial.println("Display: Android host");
  Serial.println("----------------------------------------");

  sendReady();

  Serial.println("PROTOCOL,USB_SERIAL_TEXT_V1");
  Serial.println("Type HELP for commands.");
  Serial.println("----------------------------------------");
}

void loop() {
  // Host commands from Android/PC.
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    handleCommand(command);
  }

  // Continuous controller reports.
  if (inputStreaming && millis() - lastInputReport >= INPUT_REPORT_MS) {
    lastInputReport = millis();
    sendJoystick();
  }

  // Keep the diagnostic heartbeat available during development.
  if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = millis();

    Serial.print("HEARTBEAT,");
    Serial.print("uptime=");
    Serial.print((millis() - bootTime) / 1000UL);
    Serial.print(",heap=");
    Serial.println(ESP.getFreeHeap());
  }

  delay(2);
}
