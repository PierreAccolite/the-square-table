/*
 * ESP32 GARDEN CONTROLLER
 * Standalone day/night garden controller
 *
 * OLED: SSD1306 128x32 I2C
 *
 * OLED: SDA GPIO21, SCL GPIO22
 * Joystick: X GPIO32, Y GPIO33, SW GPIO25
 * Light sensor: GPIO34 (ADC1) - connect module AO here
 * Garden relay: GPIO26
 * Irrigation relay: GPIO27 (reserved)
 *
 * Wi-Fi and Bluetooth are intentionally NOT used.
 * UI is designed specifically for a 128x32 OLED.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_SDA 21
#define OLED_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

// These are the pins from your known-working joystick sketch.
#define JOY_X 32
#define JOY_Y 33
#define JOY_BTN 25

// ESP32 ADC1 input, now moved off the joystick pins.
#define LIGHT_PIN 34

#define GARDEN_RELAY 26
#define IRRIGATION_RELAY 27

#define RELAY_ON LOW
#define RELAY_OFF HIGH

// Battery sensing remains disabled until the board's battery circuit
// is identified. Never connect the battery directly to an ADC pin.
#define BATTERY_SENSE_PIN -1
#define BATTERY_DIVIDER_RATIO 2.0f

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

uint8_t oledAddress = 0;

int darkThreshold = 1500;
int lightHysteresis = 150;
int lightRaw = 0;

bool isDark = false;
bool gardenLights = false;

enum ControlMode { MODE_AUTO, MODE_FORCE_ON, MODE_FORCE_OFF };
ControlMode gardenMode = MODE_AUTO;

enum Page {
  PAGE_HOME,
  PAGE_LIGHT,
  PAGE_OUTPUTS,
  PAGE_BATTERY,
  PAGE_SETTINGS,
  PAGE_COUNT
};

Page currentPage = PAGE_HOME;

unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastButtonTime = 0;
unsigned long lastJoystickDebug = 0;

const unsigned long SENSOR_INTERVAL = 1000;
const unsigned long DISPLAY_INTERVAL = 250;
const unsigned long BUTTON_DEBOUNCE = 100;
const unsigned long JOYSTICK_DEBUG_INTERVAL = 1000;

// Your working joystick sketch showed these centres.
// We use them as the initial calibration rather than assuming 2048.
int joyCenterX = 2870;
int joyCenterY = 2780;

const int JOY_DEADZONE = 300;

// One movement = one page. Stick must return to centre before
// another movement is accepted.
bool joystickReady = true;
bool lastButtonState = HIGH;

const char *modeName();

bool i2cDeviceExists(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void findOLED() {
  if (i2cDeviceExists(0x3C)) oledAddress = 0x3C;
  else if (i2cDeviceExists(0x3D)) oledAddress = 0x3D;
  else oledAddress = 0;
}

void setGardenRelay(bool on) {
  gardenLights = on;
  digitalWrite(GARDEN_RELAY, on ? RELAY_ON : RELAY_OFF);
}

void setIrrigationRelay(bool on) {
  digitalWrite(IRRIGATION_RELAY, on ? RELAY_ON : RELAY_OFF);
}

void readSensors() {
  lightRaw = analogRead(LIGHT_PIN);

  if (!isDark) {
    if (lightRaw < darkThreshold) isDark = true;
  } else {
    if (lightRaw > darkThreshold + lightHysteresis) isDark = false;
  }

  if (gardenMode == MODE_AUTO) setGardenRelay(isDark);
  else if (gardenMode == MODE_FORCE_ON) setGardenRelay(true);
  else setGardenRelay(false);
}

void drawTitle(const char *title, int pageNumber) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(title);
  display.setCursor(105, 0);
  display.print(pageNumber);
  display.print("/");
  display.print(PAGE_COUNT);
  display.drawLine(0, 8, 127, 8, SSD1306_WHITE);
}

void drawHome() {
  drawTitle("GARDEN", 1);
  display.setCursor(0, 11);
  display.print("LIGHT ");
  display.print(lightRaw);
  display.setCursor(0, 19);
  display.print(gardenLights ? "LAMP ON " : "LAMP OFF ");
  display.print(modeName());
}

void drawLightPage() {
  drawTitle("LIGHT", 2);
  display.setCursor(0, 11);
  display.print("RAW ");
  display.print(lightRaw);
  display.setCursor(72, 11);
  display.print("THR ");
  display.print(darkThreshold);
  display.setCursor(0, 19);
  display.print("STATE ");
  display.print(isDark ? "DARK" : "LIGHT");
}

void drawOutputsPage() {
  drawTitle("OUTPUTS", 3);
  display.setCursor(0, 11);
  display.print("GARDEN ");
  display.print(gardenLights ? "ON " : "OFF");
  display.print(modeName());
  display.setCursor(0, 19);
  display.print("IRRIG OFF");
}

float readBatteryVoltage() {
#if BATTERY_SENSE_PIN >= 0
  int raw = analogRead(BATTERY_SENSE_PIN);
  float pinVoltage = ((float)raw / 4095.0f) * 3.3f;
  return pinVoltage * BATTERY_DIVIDER_RATIO;
#else
  return NAN;
#endif
}

void drawBatteryPage() {
  drawTitle("BATTERY", 4);
  float batteryVoltage = readBatteryVoltage();
  display.setCursor(0, 11);
  display.print("POWER BATTERY");
  display.setCursor(0, 19);
  display.print("VOLT ");
  if (isnan(batteryVoltage)) display.print("N/S");
  else {
    display.print(batteryVoltage, 2);
    display.print("V");
  }
}

void drawSettingsPage() {
  drawTitle("SETTINGS", 5);
  display.setCursor(0, 11);
  display.print("THR ");
  display.print(darkThreshold);
  display.setCursor(70, 11);
  display.print("HYS ");
  display.print(lightHysteresis);
  display.setCursor(0, 19);
  display.print("LIGHT THRESHOLD");
}

void drawDisplay() {
  if (!oledAddress) return;

  switch (currentPage) {
    case PAGE_HOME: drawHome(); break;
    case PAGE_LIGHT: drawLightPage(); break;
    case PAGE_OUTPUTS: drawOutputsPage(); break;
    case PAGE_BATTERY: drawBatteryPage(); break;
    case PAGE_SETTINGS: drawSettingsPage(); break;
    default:
      currentPage = PAGE_HOME;
      drawHome();
      break;
  }

  display.display();
}

void nextPage(int direction) {
  int page = (int)currentPage + direction;
  if (page < 0) page = PAGE_COUNT - 1;
  if (page >= PAGE_COUNT) page = 0;
  currentPage = (Page)page;

  Serial.print("MENU -> ");
  Serial.print((int)currentPage + 1);
  Serial.print("/");
  Serial.println(PAGE_COUNT);
}

void handleJoystick() {
  int x = analogRead(JOY_X);
  int y = analogRead(JOY_Y);

  int dx = x - joyCenterX;
  int dy = y - joyCenterY;

  bool centred = abs(dx) < JOY_DEADZONE && abs(dy) < JOY_DEADZONE;

  if (centred) {
    joystickReady = true;
    return;
  }

  if (!joystickReady) return;

  // Vertical movement controls pages.
  if (abs(dy) > abs(dx)) {
    if (dy < -JOY_DEADZONE) {
      nextPage(-1);
      joystickReady = false;
      return;
    }
    if (dy > JOY_DEADZONE) {
      nextPage(1);
      joystickReady = false;
      return;
    }
  }

  // Horizontal movement changes the light threshold only where useful.
  if (abs(dx) >= abs(dy)) {
    if (dx < -JOY_DEADZONE) {
      if (currentPage == PAGE_LIGHT || currentPage == PAGE_SETTINGS) {
        darkThreshold = max(100, darkThreshold - 50);
        Serial.print("Threshold=");
        Serial.println(darkThreshold);
      }
      joystickReady = false;
      return;
    }

    if (dx > JOY_DEADZONE) {
      if (currentPage == PAGE_LIGHT || currentPage == PAGE_SETTINGS) {
        darkThreshold = min(4000, darkThreshold + 50);
        Serial.print("Threshold=");
        Serial.println(darkThreshold);
      }
      joystickReady = false;
      return;
    }
  }
}

void handleJoystickButton() {
  bool button = digitalRead(JOY_BTN);
  unsigned long now = millis();

  if (button == LOW && lastButtonState == HIGH &&
      now - lastButtonTime > BUTTON_DEBOUNCE) {

    lastButtonTime = now;

    if (currentPage == PAGE_OUTPUTS) {
      if (gardenMode == MODE_AUTO) gardenMode = MODE_FORCE_ON;
      else if (gardenMode == MODE_FORCE_ON) gardenMode = MODE_FORCE_OFF;
      else gardenMode = MODE_AUTO;

      readSensors();

      Serial.print("Garden mode=");
      Serial.println(modeName());
    }
  }

  lastButtonState = button;
}

void printDiagnostics() {
  int x = analogRead(JOY_X);
  int y = analogRead(JOY_Y);
  int button = digitalRead(JOY_BTN);

  Serial.print("LIGHT=");
  Serial.print(lightRaw);
  Serial.print("  JOY_X=");
  Serial.print(x);
  Serial.print("  JOY_Y=");
  Serial.print(y);
  Serial.print("  BTN=");
  Serial.print(button);
  Serial.print("  PAGE=");
  Serial.print((int)currentPage + 1);
  Serial.print("/");
  Serial.print(PAGE_COUNT);
  Serial.print("  STATE=");
  Serial.print(isDark ? "DARK" : "LIGHT");
  Serial.print("  GARDEN=");
  Serial.print(gardenLights ? "ON" : "OFF");
  Serial.print("  MODE=");
  Serial.println(modeName());
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("================================");
  Serial.println(" ESP32 GARDEN CONTROLLER");
  Serial.println("================================");

  pinMode(GARDEN_RELAY, OUTPUT);
  pinMode(IRRIGATION_RELAY, OUTPUT);
  setGardenRelay(false);
  setIrrigationRelay(false);

  pinMode(JOY_BTN, INPUT_PULLUP);

  analogReadResolution(12);

  pinMode(JOY_X, INPUT);
  pinMode(JOY_Y, INPUT);
  pinMode(LIGHT_PIN, INPUT);

  Wire.begin(OLED_SDA, OLED_SCL);
  findOLED();

  if (oledAddress) {
    Serial.print("OLED found at 0x");
    Serial.println(oledAddress, HEX);

    if (!display.begin(SSD1306_SWITCHCAPVCC, oledAddress)) {
      Serial.println("OLED initialization failed.");
      oledAddress = 0;
    } else {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println("GARDEN CTRL");
      display.println("Starting...");
      display.display();
      delay(1000);
    }
  } else {
    Serial.println("No SSD1306 OLED found at 0x3C/0x3D.");
  }

  // Show the actual joystick centre at startup.
  Serial.print("Joystick centre X=");
  Serial.print(joyCenterX);
  Serial.print(" Y=");
  Serial.println(joyCenterY);

  readSensors();
  drawDisplay();

  Serial.println("Controller ready.");
  Serial.println("Known-good joystick pins: X32 Y33 BTN25");
  Serial.println("Light sensor AO: GPIO34");
}

void loop() {
  unsigned long now = millis();

  handleJoystick();
  handleJoystickButton();

  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readSensors();
  }

  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = now;
    drawDisplay();
  }

  if (now - lastJoystickDebug >= JOYSTICK_DEBUG_INTERVAL) {
    lastJoystickDebug = now;
    printDiagnostics();
  }

  delay(5);
}

const char *modeName() {
  switch (gardenMode) {
    case MODE_AUTO: return "AUTO";
    case MODE_FORCE_ON: return "ON";
    case MODE_FORCE_OFF: return "OFF";
  }
  return "?";
}
