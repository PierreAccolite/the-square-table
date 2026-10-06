/*
 * ESP32 GARDEN CONTROLLER
 * Standalone day/night garden controller
 *
 * OLED: SSD1306 128x32 I2C
 *
 * OLED: SDA GPIO21, SCL GPIO22
 * Joystick: X GPIO35, Y GPIO34, SW GPIO33
 * Light sensor: GPIO32 (ADC1) - connect the module's AO pin here
 * Garden relay: GPIO25
 * Irrigation relay: GPIO26 (reserved)
 *
 * Wi-Fi and Bluetooth are intentionally NOT used.
 *
 * UI is designed specifically for a 128x32 OLED.
 * The screen stays on the selected page and only changes when
 * the joystick is deliberately moved.
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

// Battery sensing deliberately disabled until the board's battery
// sense circuit is identified. Do NOT connect the battery directly
// to an ESP32 ADC pin.
#define BATTERY_SENSE_PIN -1
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
unsigned long lastJoystickDebug = 0;

const unsigned long SENSOR_INTERVAL = 1000;
const unsigned long DISPLAY_INTERVAL = 250;
const unsigned long BUTTON_DEBOUNCE = 300;
const unsigned long JOYSTICK_DEBUG_INTERVAL = 1000;

// Joystick calibration.
// The joystick is normally around 2048 on a 12-bit ESP32 ADC,
// but modules/boards can have a different centre. We capture the
// actual centre at startup.
int joyCenterX = 2048;
int joyCenterY = 2048;

const int JOY_DEADZONE = 650;

// A direction is accepted once. The stick must return to centre
// before another movement is accepted.
bool joystickReady = true;

bool lastButtonState = HIGH;

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
  // The photoresistor module's analogue output (AO) goes to GPIO32.
  lightRaw = analogRead(LIGHT_PIN);

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

void calibrateJoystick() {
  // Keep the stick centred during startup.
  long totalX = 0;
  long totalY = 0;

  for (int i = 0; i < 40; i++) {
    totalX += analogRead(JOY_X);
    totalY += analogRead(JOY_Y);
    delay(5);
  }

  joyCenterX = totalX / 40;
  joyCenterY = totalY / 40;

  Serial.print("Joystick centre X=");
  Serial.print(joyCenterX);
  Serial.print(" Y=");
  Serial.println(joyCenterY);
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
  if (isnan(batteryVoltage)) {
    display.print("N/S");
  } else {
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

  bool centred =
    abs(dx) < JOY_DEADZONE &&
    abs(dy) < JOY_DEADZONE;

  // Re-arm only after the stick has actually returned to centre.
  if (centred) {
    joystickReady = true;
    return;
  }

  if (!joystickReady) return;

  // Whichever axis is moved furthest wins.
  // This also makes the control tolerant if X/Y are physically swapped.
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
  } else {
    if (dx < -JOY_DEADZONE) {
      // Horizontal joystick movement adjusts the light threshold
      // only on pages where that setting makes sense.
      if (currentPage == PAGE_LIGHT || currentPage == PAGE_SETTINGS) {
        darkThreshold -= 50;
        if (darkThreshold < 100) darkThreshold = 100;
        Serial.print("Threshold=");
        Serial.println(darkThreshold);
      }
      joystickReady = false;
      return;
    }

    if (dx > JOY_DEADZONE) {
      if (currentPage == PAGE_LIGHT || currentPage == PAGE_SETTINGS) {
        darkThreshold += 50;
        if (darkThreshold > 4000) darkThreshold = 4000;
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
      if (gardenMode == MODE_AUTO) {
        gardenMode = MODE_FORCE_ON;
      } else if (gardenMode == MODE_FORCE_ON) {
        gardenMode = MODE_FORCE_OFF;
      } else {
        gardenMode = MODE_AUTO;
      }

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

  Serial.print("LIGHT=");
  Serial.print(lightRaw);
  Serial.print("  JOY_X=");
  Serial.print(x);
  Serial.print("  JOY_Y=");
  Serial.print(y);
  Serial.print("  BTN=");
  Serial.print(digitalRead(JOY_BTN));
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

  // Safe startup: outputs OFF.
  setGardenRelay(false);
  setIrrigationRelay(false);

  pinMode(JOY_BTN, INPUT_PULLUP);

  analogReadResolution(12);

  // GPIO32 is ADC1 and is suitable for analogue light sensing.
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

  // IMPORTANT: leave the joystick centred during this calibration.
  calibrateJoystick();

  readSensors();
  drawDisplay();

  Serial.println("Controller ready.");
  Serial.println("Move joystick and watch JOY_X / JOY_Y values.");
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
    case MODE_AUTO:      return "AUTO";
    case MODE_FORCE_ON:  return "ON";
    case MODE_FORCE_OFF: return "OFF";
  }
  return "?";
}
