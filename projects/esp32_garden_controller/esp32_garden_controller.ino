/*
 * ESP32 GARDEN CONTROLLER
 * Standalone day/night garden controller
 *
 * OLED: SSD1306 128x32 I2C
 *
 * OLED: SDA GPIO21, SCL GPIO22
 * Joystick: X GPIO35, Y GPIO34, SW GPIO33
 * Light sensor: GPIO32 (ADC1)
 * Garden relay: GPIO25
 * Irrigation relay: GPIO26 (reserved)
 *
 * Wi-Fi and Bluetooth are intentionally NOT used.
 *
 * UI is designed specifically for a 128x32 OLED.
 * The screen stays on the selected page; it only changes page
 * when the joystick is moved. A direction must return to centre
 * before another page move is accepted.
 *
 * Temperature/DHT support is intentionally left out for now.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_SDA 21
#define OLED_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

#define JOY_X 35
#define JOY_Y 34
#define JOY_BTN 33

#define LIGHT_PIN 32

#define GARDEN_RELAY 25
#define IRRIGATION_RELAY 26

// Most common relay modules are active LOW.
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// IMPORTANT:
// Battery voltage cannot safely be measured directly by an ESP32 ADC pin.
// This is left disabled until the exact ESP32 board/battery-sense circuit
// is identified. Do NOT connect the battery directly to an ADC pin.
#define BATTERY_SENSE_PIN -1

// Set this only after a proper resistor-divider/battery-sense input is confirmed.
#define BATTERY_DIVIDER_RATIO 2.0f

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

uint8_t oledAddress = 0;

int darkThreshold = 1500;
int lightHysteresis = 150;

int lightRaw = 0;

bool isDark = false;
bool gardenLights = false;

enum ControlMode {
  MODE_AUTO,
  MODE_FORCE_ON,
  MODE_FORCE_OFF
};

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

const unsigned long SENSOR_INTERVAL = 1000;
const unsigned long DISPLAY_INTERVAL = 250;
const unsigned long BUTTON_DEBOUNCE = 300;

bool lastButtonState = HIGH;

// Joystick centre/dead-zone.
const int JOY_CENTER = 2048;
const int JOY_DEADZONE = 700;

// A joystick direction is accepted once, then must return to centre.
// This makes the menu deliberately static instead of scrolling.
bool joystickReady = true;

const char *modeName();

bool i2cDeviceExists(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void findOLED() {
  if (i2cDeviceExists(0x3C)) {
    oledAddress = 0x3C;
  } else if (i2cDeviceExists(0x3D)) {
    oledAddress = 0x3D;
  } else {
    oledAddress = 0;
  }
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

  // Hysteresis prevents relay chatter around the threshold.
  if (!isDark) {
    if (lightRaw < darkThreshold) isDark = true;
  } else {
    if (lightRaw > darkThreshold + lightHysteresis) isDark = false;
  }

  if (gardenMode == MODE_AUTO) {
    setGardenRelay(isDark);
  } else if (gardenMode == MODE_FORCE_ON) {
    setGardenRelay(true);
  } else {
    setGardenRelay(false);
  }
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

void drawFooter(const char *text) {
  display.setCursor(0, 24);
  display.print(text);
}

void drawHome() {
  drawTitle("GARDEN", 1);

  display.setCursor(0, 11);
  display.print("LIGHT ");
  display.print(lightRaw);

  display.setCursor(0, 19);
  display.print(gardenLights ? "LAMP ON " : "LAMP OFF ");
  display.print(modeName());

  drawFooter("^v MENU  PRESS");
}

void drawLightPage() {
  drawTitle("LIGHT", 2);

  display.setCursor(0, 11);
  display.print("RAW ");
  display.print(lightRaw);
  display.print("  THR ");
  display.print(darkThreshold);

  display.setCursor(0, 19);
  display.print("STATE ");
  display.print(isDark ? "DARK" : "LIGHT");

  drawFooter("^v MENU  X +/-");
}

void drawOutputsPage() {
  drawTitle("OUTPUTS", 3);

  display.setCursor(0, 11);
  display.print("GARDEN ");
  display.print(gardenLights ? "ON " : "OFF");
  display.print(modeName());

  display.setCursor(0, 19);
  display.print("IRRIG OFF");

  drawFooter("PRESS = AUTO/ON/OFF");
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
  if (isnan(batteryVoltage)) {
    display.print("N/S");
  } else {
    display.print(batteryVoltage, 2);
    display.print("V");
  }

  drawFooter("UP ");
  unsigned long seconds = millis() / 1000UL;
  unsigned long minutes = seconds / 60UL;
  unsigned long hours = minutes / 60UL;
  display.print(hours);
  display.print("h ");
  display.print(minutes % 60UL);
  display.print("m");
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
  display.print("X -/+ THRESHOLD");

  drawFooter("^v MENU");
}

void drawDisplay() {
  if (!oledAddress) return;

  switch (currentPage) {
    case PAGE_HOME:     drawHome(); break;
    case PAGE_LIGHT:    drawLightPage(); break;
    case PAGE_OUTPUTS:  drawOutputsPage(); break;
    case PAGE_BATTERY:  drawBatteryPage(); break;
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
}

void handleJoystick() {
  int x = analogRead(JOY_X);
  int y = analogRead(JOY_Y);

  bool xLeft  = x < JOY_CENTER - JOY_DEADZONE;
  bool xRight = x > JOY_CENTER + JOY_DEADZONE;
  bool yUp    = y < JOY_CENTER - JOY_DEADZONE;
  bool yDown  = y > JOY_CENTER + JOY_DEADZONE;

  // Require the stick to return to centre before another movement.
  if (!xLeft && !xRight && !yUp && !yDown) {
    joystickReady = true;
    return;
  }

  if (!joystickReady) return;

  if (yUp) {
    nextPage(-1);
    joystickReady = false;
    return;
  }

  if (yDown) {
    nextPage(1);
    joystickReady = false;
    return;
  }

  if (currentPage == PAGE_LIGHT || currentPage == PAGE_SETTINGS) {
    if (xLeft) {
      darkThreshold -= 50;
      if (darkThreshold < 100) darkThreshold = 100;
      joystickReady = false;
    } else if (xRight) {
      darkThreshold += 50;
      if (darkThreshold > 4000) darkThreshold = 4000;
      joystickReady = false;
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
      if (gardenMode == MODE_AUTO) {
        gardenMode = MODE_FORCE_ON;
      } else if (gardenMode == MODE_FORCE_ON) {
        gardenMode = MODE_FORCE_OFF;
      } else {
        gardenMode = MODE_AUTO;
      }

      readSensors();
    }
  }

  lastButtonState = button;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("================================");
  Serial.println(" ESP32 GARDEN CONTROLLER");
  Serial.println("================================");

  pinMode(GARDEN_RELAY, OUTPUT);
  pinMode(IRRIGATION_RELAY, OUTPUT);

  // Safe startup: outputs OFF.
  setGardenRelay(false);
  setIrrigationRelay(false);

  pinMode(JOY_BTN, INPUT_PULLUP);

  analogReadResolution(12);

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

  readSensors();
  drawDisplay();

  Serial.println("Controller ready.");
}

void loop() {
  unsigned long now = millis();

  handleJoystick();
  handleJoystickButton();

  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readSensors();

    Serial.print("Light=");
    Serial.print(lightRaw);
    Serial.print(" State=");
    Serial.print(isDark ? "NIGHT" : "DAY");
    Serial.print(" Garden=");
    Serial.print(gardenLights ? "ON" : "OFF");
    Serial.print(" Mode=");
    Serial.println(modeName());
  }

  // Refresh data, but never automatically change pages.
  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = now;
    drawDisplay();
  }

  delay(5);
}

const char *modeName() {
  switch (gardenMode) {
    case MODE_AUTO:      return "AUTO";
    case MODE_FORCE_ON:  return "ON";
    case MODE_FORCE_OFF: return "OFF";
  }
  return "?";
}
