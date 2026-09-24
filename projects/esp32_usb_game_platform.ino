/*
  POCKET ARCADE - USB GAME PLATFORM
  ----------------------------------
  Classic ESP32-D0WDQ6

  USB game/input platform for the Pocket Arcade.
  The browser uses the proven USB serial link rate internally.
*/

#include <Arduino.h>
#include <Preferences.h>

static const int JOY_X_PIN = 32;
static const int JOY_Y_PIN = 33;
static const int JOY_BTN_PIN = 25;

static const uint32_t USB_BAUD = 115200;
static const uint32_t HEARTBEAT_MS = 2000;
static const uint32_t INPUT_REPORT_MS = 50;

static const int DEFAULT_X_MIN = 0;
static const int DEFAULT_X_MAX = 4095;
static const int DEFAULT_Y_MIN = 0;
static const int DEFAULT_Y_MAX = 4095;

static const int DEFAULT_X_CENTER = 2870;
static const int DEFAULT_Y_CENTER = 2780;
static const int JOY_DEADZONE = 8;

int joyXMin = DEFAULT_X_MIN;
int joyXMax = DEFAULT_X_MAX;
int joyYMin = DEFAULT_Y_MIN;
int joyYMax = DEFAULT_Y_MAX;
int joyXCenter = DEFAULT_X_CENTER;
int joyYCenter = DEFAULT_Y_CENTER;

Preferences preferences;

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
  if (value >= center) {
    result = map(value, center, maximum, 0, 100);
  } else {
    result = map(value, minimum, center, -100, 0);
  }

  if (abs((int)result) <= JOY_DEADZONE) return 0;
  return clampInt((int)result, -100, 100);
}

void loadCalibration() {
  preferences.begin("pocketjoy", true);

  joyXMin = preferences.getInt("xmin", DEFAULT_X_MIN);
  joyXMax = preferences.getInt("xmax", DEFAULT_X_MAX);
  joyYMin = preferences.getInt("ymin", DEFAULT_Y_MIN);
  joyYMax = preferences.getInt("ymax", DEFAULT_Y_MAX);
  joyXCenter = preferences.getInt("xctr", DEFAULT_X_CENTER);
  joyYCenter = preferences.getInt("yctr", DEFAULT_Y_CENTER);

  preferences.end();

  if (joyXMax <= joyXMin || joyXCenter <= joyXMin || joyXCenter >= joyXMax) {
    joyXMin = DEFAULT_X_MIN;
    joyXMax = DEFAULT_X_MAX;
    joyXCenter = DEFAULT_X_CENTER;
  }

  if (joyYMax <= joyYMin || joyYCenter <= joyYMin || joyYCenter >= joyYMax) {
    joyYMin = DEFAULT_Y_MIN;
    joyYMax = DEFAULT_Y_MAX;
    joyYCenter = DEFAULT_Y_CENTER;
  }
}

void saveCalibration() {
  preferences.begin("pocketjoy", false);

  preferences.putInt("xmin", joyXMin);
  preferences.putInt("xmax", joyXMax);
  preferences.putInt("ymin", joyYMin);
  preferences.putInt("ymax", joyYMax);
  preferences.putInt("xctr", joyXCenter);
  preferences.putInt("yctr", joyYCenter);

  preferences.end();
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

  Serial.print("STATUS,chip=");
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
  Serial.print(inputStreaming ? "1" : "0");
  Serial.print(",cal=");
  Serial.print(joyXMin);
  Serial.print(":");
  Serial.print(joyXMax);
  Serial.print(":");
  Serial.print(joyXCenter);
  Serial.print(":");
  Serial.print(joyYMin);
  Serial.print(":");
  Serial.print(joyYMax);
  Serial.print(":");
  Serial.println(joyYCenter);
}

void sendJoystick() {
  readController();

  int x = normalizeAxis(controller.rawX, joyXMin, joyXMax, joyXCenter);
  int y = normalizeAxis(controller.rawY, joyYMin, joyYMax, joyYCenter);

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

// Guided automatic calibration:
// 1 second: leave stick centered
// 6 seconds: move the stick through its full range repeatedly
void runCalibration() {
  inputStreaming = false;

  Serial.println("CAL,START");
  Serial.println("CAL,CENTER,KEEP_STICK_CENTERED");

  long xTotal = 0;
  long yTotal = 0;
  const int centerSamples = 100;

  for (int i = 0; i < centerSamples; i++) {
    xTotal += analogRead(JOY_X_PIN);
    yTotal += analogRead(JOY_Y_PIN);
    delay(10);
  }

  const int newXCenter = (int)(xTotal / centerSamples);
  const int newYCenter = (int)(yTotal / centerSamples);

  Serial.print("CAL,CENTER_CAPTURED,");
  Serial.print(newXCenter);
  Serial.print(",");
  Serial.println(newYCenter);

  Serial.println("CAL,RANGE,MOVE_STICK_FULL_RANGE");

  int newXMin = 4095;
  int newXMax = 0;
  int newYMin = 4095;
  int newYMax = 0;

  const unsigned long rangeStart = millis();
  while (millis() - rangeStart < 6000UL) {
    int x = analogRead(JOY_X_PIN);
    int y = analogRead(JOY_Y_PIN);

    if (x < newXMin) newXMin = x;
    if (x > newXMax) newXMax = x;
    if (y < newYMin) newYMin = y;
    if (y > newYMax) newYMax = y;

    delay(10);
  }

  if (newXMax - newXMin < 200 || newYMax - newYMin < 200) {
    Serial.println("CAL,ERROR,NOT_ENOUGH_RANGE");
    return;
  }

  joyXMin = newXMin;
  joyXMax = newXMax;
  joyYMin = newYMin;
  joyYMax = newYMax;
  joyXCenter = newXCenter;
  joyYCenter = newYCenter;

  saveCalibration();

  Serial.print("CAL,DONE,X=");
  Serial.print(joyXMin);
  Serial.print(":");
  Serial.print(joyXMax);
  Serial.print(":");
  Serial.print(joyXCenter);
  Serial.print(",Y=");
  Serial.print(joyYMin);
  Serial.print(":");
  Serial.print(joyYMax);
  Serial.print(":");
  Serial.println(joyYCenter);
}

void handleCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

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
  else if (upper == "CALIBRATE") {
    runCalibration();
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
    Serial.println("HELP,PING|STATUS|JOY|JOYRAW|INPUT|INPUT OFF|CALIBRATE|START|SELECT|MENU|BACK|HELP");
  }
  else {
    Serial.print("ERROR,UNKNOWN_COMMAND,");
    Serial.println(command);
  }
}

void setup() {
  pinMode(JOY_BTN_PIN, INPUT_PULLUP);
  analogReadResolution(12);

  loadCalibration();

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
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    handleCommand(command);
  }

  if (inputStreaming && millis() - lastInputReport >= INPUT_REPORT_MS) {
    lastInputReport = millis();
    sendJoystick();
  }

  if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = millis();

    Serial.print("HEARTBEAT,uptime=");
    Serial.print((millis() - bootTime) / 1000UL);
    Serial.print(",heap=");
    Serial.println(ESP.getFreeHeap());
  }

  delay(2);
}
