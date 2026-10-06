/*
 * ESP32 GARDEN CONTROLLER
 * Standalone day/night garden controller
 *
 * ESP32 Dev Module / classic ESP32-WROOM style board
 * OLED: SSD1306 128x64 I2C
 *
 * OLED: SDA GPIO21, SCL GPIO22
 * Joystick: X GPIO35, Y GPIO34, SW GPIO33
 * Light sensor: GPIO32 (ADC1)
 * DHT11: GPIO27
 * Garden relay: GPIO25
 * Irrigation relay: GPIO26 (reserved)
 *
 * Wi-Fi and Bluetooth are intentionally NOT used.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define OLED_SDA 21
#define OLED_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define JOY_X 35
#define JOY_Y 34
#define JOY_BTN 33

#define LIGHT_PIN 32
#define DHT_PIN 27
#define DHT_TYPE DHT11

#define GARDEN_RELAY 25
#define IRRIGATION_RELAY 26

// Most common relay modules are active LOW.
#define RELAY_ON LOW
#define RELAY_OFF HIGH

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);

uint8_t oledAddress = 0;

int darkThreshold = 1500;
int lightHysteresis = 150;

int lightRaw = 0;
float temperatureC = NAN;
float humidity = NAN;

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
  PAGE_TEMP,
  PAGE_OUTPUTS,
  PAGE_SETTINGS
};

Page currentPage = PAGE_HOME;

unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastButtonTime = 0;

const unsigned long SENSOR_INTERVAL = 2000;
const unsigned long DISPLAY_INTERVAL = 150;
const unsigned long BUTTON_DEBOUNCE = 300;

bool lastButtonState = HIGH;

const int JOY_CENTER = 2048;
const int JOY_DEADZONE = 700;

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

  float newTemp = dht.readTemperature();
  float newHumidity = dht.readHumidity();

  if (!isnan(newTemp)) temperatureC = newTemp;
  if (!isnan(newHumidity)) humidity = newHumidity;

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

void drawHeader(const char *title) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(title);

  display.setCursor(91, 0);
  display.print(isDark ? "NIGHT" : "DAY");

  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
}

void drawHome() {
  drawHeader("GARDEN CTRL");

  display.setCursor(0, 15);
  display.print("Light: ");
  display.print(lightRaw);

  display.setCursor(0, 27);
  display.print("Temp : ");
  if (isnan(temperatureC)) display.print("--");
  else {
    display.print(temperatureC, 1);
    display.print(" C");
  }

  display.setCursor(0, 39);
  display.print("Hum  : ");
  if (isnan(humidity)) display.print("--");
  else {
    display.print(humidity, 0);
    display.print("%");
  }

  display.setCursor(0, 51);
  display.print("Garden: ");
  display.print(gardenLights ? "ON" : "OFF");

  display.setCursor(89, 51);
  display.print(modeName());
}

void drawLightPage() {
  drawHeader("LIGHT SENSOR");

  display.setCursor(0, 16);
  display.print("RAW: ");
  display.print(lightRaw);

  display.setCursor(0, 29);
  display.print("THR: ");
  display.print(darkThreshold);

  display.setCursor(0, 42);
  display.print("STATE: ");
  display.print(isDark ? "DARK" : "LIGHT");

  display.setCursor(0, 55);
  display.print("L/R: threshold");
}

void drawTempPage() {
  drawHeader("TEMPERATURE");

  display.setCursor(0, 17);
  display.print("Temp: ");
  if (isnan(temperatureC)) display.print("NO DATA");
  else {
    display.print(temperatureC, 1);
    display.print(" C");
  }

  display.setCursor(0, 31);
  display.print("Humidity: ");
  if (isnan(humidity)) display.print("NO DATA");
  else {
    display.print(humidity, 0);
    display.print("%");
  }

  display.setCursor(0, 48);
  display.print("DHT11");
}

const char *modeName() {
  switch (gardenMode) {
    case MODE_AUTO: return "AUTO";
    case MODE_FORCE_ON: return "ON";
    case MODE_FORCE_OFF: return "OFF";
  }
  return "?";
}

void drawOutputsPage() {
  drawHeader("OUTPUTS");

  display.setCursor(0, 17);
  display.print("Garden: ");
  display.print(gardenLights ? "ON" : "OFF");

  display.setCursor(0, 30);
  display.print("Mode:   ");
  display.print(modeName());

  display.setCursor(0, 43);
  display.print("Irrig:  OFF");

  display.setCursor(0, 56);
  display.print("PRESS: change mode");
}

void drawSettingsPage() {
  drawHeader("SETTINGS");

  display.setCursor(0, 17);
  display.print("Dark threshold:");

  display.setCursor(0, 30);
  display.print(darkThreshold);

  display.setCursor(0, 43);
  display.print("Hysteresis: ");
  display.print(lightHysteresis);

  display.setCursor(0, 56);
  display.print("L/R: threshold");
}

void drawDisplay() {
  if (!oledAddress) return;

  switch (currentPage) {
    case PAGE_HOME: drawHome(); break;
    case PAGE_LIGHT: drawLightPage(); break;
    case PAGE_TEMP: drawTempPage(); break;
    case PAGE_OUTPUTS: drawOutputsPage(); break;
    case PAGE_SETTINGS: drawSettingsPage(); break;
  }

  display.display();
}

void nextPage(int direction) {
  int page = (int)currentPage + direction;

  if (page < 0) page = 4;
  if (page > 4) page = 0;

  currentPage = (Page)page;
}

void handleJoystick() {
  int x = analogRead(JOY_X);
  int y = analogRead(JOY_Y);

  if (y < JOY_CENTER - JOY_DEADZONE) {
    nextPage(-1);
    delay(120);
  } else if (y > JOY_CENTER + JOY_DEADZONE) {
    nextPage(1);
    delay(120);
  }

  if (currentPage == PAGE_LIGHT || currentPage == PAGE_SETTINGS) {
    if (x < JOY_CENTER - JOY_DEADZONE) {
      darkThreshold -= 50;
      if (darkThreshold < 100) darkThreshold = 100;
      delay(80);
    } else if (x > JOY_CENTER + JOY_DEADZONE) {
      darkThreshold += 50;
      if (darkThreshold > 4000) darkThreshold = 4000;
      delay(80);
    }
  }
}

void handleJoystickButton() {
  bool button = digitalRead(JOY_BTN);

  if (button == LOW && lastButtonState == HIGH &&
      millis() - lastButtonTime > BUTTON_DEBOUNCE) {

    lastButtonTime = millis();

    if (currentPage == PAGE_OUTPUTS) {
      if (gardenMode == MODE_AUTO) gardenMode = MODE_FORCE_ON;
      else if (gardenMode == MODE_FORCE_ON) gardenMode = MODE_FORCE_OFF;
      else gardenMode = MODE_AUTO;

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
      display.println("GARDEN CONTROLLER");
      display.println();
      display.println("Starting...");
      display.display();
      delay(1000);
    }
  } else {
    Serial.println("No SSD1306 OLED found at 0x3C/0x3D.");
  }

  dht.begin();
  delay(1500);

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
    Serial.print(" Temp=");
    Serial.print(temperatureC);
    Serial.print("C Hum=");
    Serial.print(humidity);
    Serial.print("% Garden=");
    Serial.print(gardenLights ? "ON" : "OFF");
    Serial.print(" Mode=");
    Serial.println(modeName());
  }

  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = now;
    drawDisplay();
  }

  delay(5);
}
