/*
 * ESP32 GARDEN CONTROLLER
 * Standalone garden controller - workbench / field-test build
 *
 * OLED: SSD1306 128x32 I2C
 * Joystick: X GPIO32, Y GPIO33, SW GPIO25
 * Light sensor: GPIO34
 * KY-028 temperature module: A0 GPIO35, D0 GPIO39
 * Garden relay: GPIO26
 * Irrigation relay: GPIO27 (kept OFF unless explicitly enabled below)
 *
 * Wi-Fi and Bluetooth are intentionally NOT used.
 *
 * CURRENT TEMP APPROACH
 * ----------------------
 * The KY-028 analogue output is treated as an experimental sensor signal.
 * It is NOT a calibrated Celsius measurement yet.
 *
 * Starting calibration:
 *   TEMP_REFERENCE_RAW = 480
 *   TEMP_REFERENCE_C   = 22.0 C
 *   TEMP_C_PER_COUNT   = -0.05 C/count
 *
 * These numbers are deliberately easy to change after real-world testing.
 *
 * KY-028 wiring:
 *   +  -> 3.3V
 *   -  -> GND
 *   A0 -> GPIO35
 *   D0 -> GPIO39
 *
 * Battery monitoring:
 *   This board variant does not expose a documented onboard battery ADC.
 *   An optional external 100K/100K divider can be connected to GPIO36.
 *   Divider input should be the board's battery/5V rail; GPIO36 must never
 *   see more than the ESP32 ADC input range.
 *
 * Clock:
 *   Software RTC using the ESP32 system clock + Preferences.
 *   The clock continues while the board remains powered from its battery.
 *   It is not a true RTC: removing all power stops the clock.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <Preferences.h>
#include <time.h>

#define OLED_SDA 21
#define OLED_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

#define JOY_X 32
#define JOY_Y 33
#define JOY_BTN 25

#define LIGHT_PIN 34

#define TEMP_A0 35
#define TEMP_D0 39

#define GARDEN_RELAY 26
#define IRRIGATION_RELAY 27

#define RELAY_ON LOW
#define RELAY_OFF HIGH

// Optional battery monitor.
// Hardware required: 100K from battery/5V rail to GPIO36 and 100K from
// GPIO36 to GND. This gives a 2:1 divider.
#define BATTERY_SENSE_PIN 36
#define BATTERY_DIVIDER_RATIO 2.0f

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
uint8_t oledAddress = 0;
Preferences preferences;

// -------------------- Light --------------------

int darkThreshold = 1500;
int lightHysteresis = 150;
int lightRaw = 0;
bool isDark = false;

// -------------------- Temperature --------------------

// Initial field calibration only.
// Change these later when we have a trusted thermometer/reference.
float tempReferenceC = 22.0f;
int tempReferenceRaw = 480;
float tempCPerCount = -0.05f;

int tempRaw = 0;
int tempRawMin = 4095;
int tempRawMax = 0;
int tempDigital = HIGH;

float temperatureC = 22.0f;

// Small moving average makes the displayed value less jumpy.
const int TEMP_SAMPLES = 8;
int tempSamples[TEMP_SAMPLES];
int tempSampleIndex = 0;
bool tempSamplesReady = false;

// -------------------- Clock --------------------

// Software clock. It survives normal resets because the last saved epoch is
// stored in NVS, and continues ticking while the ESP32 remains powered.
// It cannot account for time spent completely without power.

enum ClockField {
  CLOCK_HOUR,
  CLOCK_MINUTE,
  CLOCK_DAY,
  CLOCK_MONTH,
  CLOCK_YEAR,
  CLOCK_FIELD_COUNT
};

ClockField clockField = CLOCK_HOUR;
bool clockSet = false;
unsigned long lastClockSave = 0;
const unsigned long CLOCK_SAVE_INTERVAL = 60000;

int daysInMonth(int year, int month) {
  if (month == 2) {
    bool leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
    return leap ? 29 : 28;
  }

  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

void clampClockDate(struct tm &t) {
  if (t.tm_year + 1900 < 2020) t.tm_year = 2020 - 1900;
  if (t.tm_year + 1900 > 2099) t.tm_year = 2099 - 1900;

  if (t.tm_mon < 0) t.tm_mon = 0;
  if (t.tm_mon > 11) t.tm_mon = 11;

  int maxDay = daysInMonth(t.tm_year + 1900, t.tm_mon + 1);
  if (t.tm_mday < 1) t.tm_mday = 1;
  if (t.tm_mday > maxDay) t.tm_mday = maxDay;

  if (t.tm_hour < 0) t.tm_hour = 0;
  if (t.tm_hour > 23) t.tm_hour = 23;
  if (t.tm_min < 0) t.tm_min = 0;
  if (t.tm_min > 59) t.tm_min = 59;
  if (t.tm_sec < 0) t.tm_sec = 0;
  if (t.tm_sec > 59) t.tm_sec = 59;
}

void saveClock() {
  time_t now = time(nullptr);
  if (now < 1000000000) return;

  preferences.putULong64("epoch", (uint64_t)now);
  preferences.putBool("clockSet", true);
  clockSet = true;
  lastClockSave = millis();
}

void loadClock() {
  uint64_t saved = preferences.getULong64("epoch", 0);
  clockSet = preferences.getBool("clockSet", false);

  if (clockSet && saved > 1000000000ULL) {
    struct timeval tv;
    tv.tv_sec = (time_t)saved;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    return;
  }

  // First run: use the firmware build date/time as a useful starting point.
  struct tm buildTime = {};
  const char *date = __DATE__;
  const char *timeStr = __TIME__;

  char monthText[4];
  int day, year, hour, minute, second;
  sscanf(date, "%3s %d %d", monthText, &day, &year);
  sscanf(timeStr, "%d:%d:%d", &hour, &minute, &second);

  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *m = strstr(months, monthText);
  int month = m ? (int)(m - months) / 3 : 0;

  buildTime.tm_year = year - 1900;
  buildTime.tm_mon = month;
  buildTime.tm_mday = day;
  buildTime.tm_hour = hour;
  buildTime.tm_min = minute;
  buildTime.tm_sec = second;
  buildTime.tm_isdst = -1;

  time_t buildEpoch = mktime(&buildTime);
  struct timeval tv;
  tv.tv_sec = buildEpoch;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  clockSet = false;
}

void adjustClock(int direction) {
  time_t now = time(nullptr);
  if (now < 1000000000) return;

  struct tm t;
  localtime_r(&now, &t);

  switch (clockField) {
    case CLOCK_HOUR:
      t.tm_hour += direction;
      break;

    case CLOCK_MINUTE:
      t.tm_min += direction;
      break;

    case CLOCK_DAY:
      t.tm_mday += direction;
      break;

    case CLOCK_MONTH:
      t.tm_mon += direction;
      break;

    case CLOCK_YEAR:
      t.tm_year += direction;
      break;

    default:
      break;
  }

  t.tm_isdst = -1;
  time_t adjusted = mktime(&t);
  localtime_r(&adjusted, &t);
  clampClockDate(t);
  adjusted = mktime(&t);

  struct timeval tv;
  tv.tv_sec = adjusted;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);

  saveClock();

  Serial.print("CLOCK -> ");
  Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n",
                t.tm_year + 1900,
                t.tm_mon + 1,
                t.tm_mday,
                t.tm_hour,
                t.tm_min,
                t.tm_sec);
}

void nextClockField() {
  clockField = (ClockField)(((int)clockField + 1) % CLOCK_FIELD_COUNT);
  Serial.print("Clock field -> ");
  switch (clockField) {
    case CLOCK_HOUR: Serial.println("HOUR"); break;
    case CLOCK_MINUTE: Serial.println("MINUTE"); break;
    case CLOCK_DAY: Serial.println("DAY"); break;
    case CLOCK_MONTH: Serial.println("MONTH"); break;
    case CLOCK_YEAR: Serial.println("YEAR"); break;
    default: Serial.println("?"); break;
  }
}

const char *clockFieldName() {
  switch (clockField) {
    case CLOCK_HOUR: return "HOUR";
    case CLOCK_MINUTE: return "MIN";
    case CLOCK_DAY: return "DAY";
    case CLOCK_MONTH: return "MONTH";
    case CLOCK_YEAR: return "YEAR";
  }
  return "?";
}

void printClock(char *buffer, size_t length) {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);

  snprintf(buffer, length, "%02d:%02d:%02d",
           t.tm_hour, t.tm_min, t.tm_sec);
}

void printClockDate(char *buffer, size_t length) {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);

  snprintf(buffer, length, "%02d/%02d/%04d",
           t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
}

// -------------------- Outputs --------------------

bool gardenLights = false;
bool irrigation = false;

enum ControlMode {
  MODE_AUTO,
  MODE_FORCE_ON,
  MODE_FORCE_OFF
};

ControlMode gardenMode = MODE_AUTO;

// Irrigation is deliberately disabled for this first field build.
// Set true only when the irrigation relay/wiring is ready.
bool irrigationControlEnabled = false;

// -------------------- Pages --------------------

enum Page {
  PAGE_HOME,
  PAGE_LIGHT,
  PAGE_OUTPUTS,
  PAGE_BATTERY,
  PAGE_CLOCK,
  PAGE_SETTINGS,
  PAGE_TEMP,
  PAGE_COUNT
};

Page currentPage = PAGE_HOME;

// -------------------- Timing --------------------

unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastButtonTime = 0;
unsigned long lastJoystickDebug = 0;
unsigned long lastTempSample = 0;

const unsigned long SENSOR_INTERVAL = 1000;
const unsigned long DISPLAY_INTERVAL = 250;
const unsigned long BUTTON_DEBOUNCE = 120;
const unsigned long JOYSTICK_DEBUG_INTERVAL = 1000;

// -------------------- Joystick --------------------

// These are the known-good centres from the current hardware test.
int joyCenterX = 1855;
int joyCenterY = 1805;

const int JOY_DEADZONE = 300;

// One physical stick movement produces one action.
// Stick must return to centre before another action.
bool joystickReady = true;

bool lastButtonState = HIGH;

// -------------------- Helpers --------------------

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
  irrigation = on;
  digitalWrite(IRRIGATION_RELAY, on ? RELAY_ON : RELAY_OFF);
}

float calculateTemperature(int raw) {
  return tempReferenceC + ((float)raw - (float)tempReferenceRaw) * tempCPerCount;
}

void readTemperature() {
  // Average several readings for a calmer value.
  long total = 0;

  for (int i = 0; i < TEMP_SAMPLES; i++) {
    total += analogRead(TEMP_A0);
    delayMicroseconds(150);
  }

  tempRaw = total / TEMP_SAMPLES;
  tempDigital = digitalRead(TEMP_D0);

  if (tempRaw < tempRawMin) tempRawMin = tempRaw;
  if (tempRaw > tempRawMax) tempRawMax = tempRaw;

  tempSamples[tempSampleIndex] = tempRaw;
  tempSampleIndex++;

  if (tempSampleIndex >= TEMP_SAMPLES) {
    tempSampleIndex = 0;
    tempSamplesReady = true;
  }

  int count = tempSamplesReady ? TEMP_SAMPLES : tempSampleIndex;

  if (count <= 0) {
    count = 1;
    tempSamples[0] = tempRaw;
  }

  long smoothTotal = 0;
  for (int i = 0; i < count; i++) {
    smoothTotal += tempSamples[i];
  }

  int smoothRaw = smoothTotal / count;
  temperatureC = calculateTemperature(smoothRaw);
}

void readLight() {
  lightRaw = analogRead(LIGHT_PIN);

  if (!isDark) {
    if (lightRaw < darkThreshold) {
      isDark = true;
    }
  } else {
    if (lightRaw > darkThreshold + lightHysteresis) {
      isDark = false;
    }
  }
}

void updateOutputs() {
  if (gardenMode == MODE_AUTO) {
    setGardenRelay(isDark);
  } else if (gardenMode == MODE_FORCE_ON) {
    setGardenRelay(true);
  } else {
    setGardenRelay(false);
  }

  // Safety-first: irrigation stays OFF until deliberately enabled in code.
  if (!irrigationControlEnabled) {
    setIrrigationRelay(false);
  }
}

void readSensors() {
  readLight();
  readTemperature();
  updateOutputs();
}

// -------------------- OLED --------------------

void drawTitle(const char *title, int pageNumber) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(title);

  display.setCursor(96, 0);
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
  display.print("TEMP ");
  display.print(temperatureC, 1);
  display.print("C ");

  display.print(gardenLights ? "ON" : "OFF");
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

  display.setCursor(78, 19);
  display.print(gardenLights ? "LAMP ON" : "LAMP OFF");
}

void drawOutputsPage() {
  drawTitle("OUTPUTS", 3);

  display.setCursor(0, 11);
  display.print("GARDEN ");
  display.print(gardenLights ? "ON " : "OFF");
  display.print(modeName());

  display.setCursor(0, 19);
  display.print("IRRIG ");
  display.print(irrigation ? "ON" : "OFF");

  display.setCursor(72, 19);
  display.print(irrigationControlEnabled ? "ENABLED" : "LOCKED");
}

float readBatteryVoltage() {
#if BATTERY_SENSE_PIN >= 0
  uint32_t mv = analogReadMilliVolts(BATTERY_SENSE_PIN);
  return ((float)mv / 1000.0f) * BATTERY_DIVIDER_RATIO;
#else
  return NAN;
#endif
}

void drawBatteryPage() {
  drawTitle("BATTERY", 4);

  display.setCursor(0, 11);
  float voltage = readBatteryVoltage();

  if (isnan(voltage)) {
    display.print("MONITOR N/S");
  } else {
    display.print("VOLT ");
    display.print(voltage, 2);
    display.print("V");
  }

  display.setCursor(0, 19);
  if (!isnan(voltage)) {
    if (voltage >= 4.15f) display.print("FULL");
    else if (voltage >= 3.85f) display.print("GOOD");
    else if (voltage >= 3.60f) display.print("LOW");
    else display.print("CRITICAL");
  } else {
    display.print("GPIO36 / 2:1");
  }
}

void drawClockPage() {
  drawTitle("CLOCK", 5);

  char clockText[16];
  char dateText[20];
  printClock(clockText, sizeof(clockText));
  printClockDate(dateText, sizeof(dateText));

  display.setCursor(0, 11);
  display.print(clockText);

  display.setCursor(72, 11);
  display.print(clockSet ? "SET" : "BUILD");

  display.setCursor(0, 19);
  display.print(dateText);

  display.setCursor(78, 19);
  display.print(clockFieldName());
}

void drawSettingsPage() {
  drawTitle("SETTINGS", 6);

  display.setCursor(0, 11);
  display.print("LTHR ");
  display.print(darkThreshold);

  display.setCursor(70, 11);
  display.print("HYS ");
  display.print(lightHysteresis);

  display.setCursor(0, 19);
  display.print("TREF ");
  display.print(tempReferenceC, 1);
  display.print("C");

  display.setCursor(76, 19);
  display.print("RAW ");
  display.print(tempReferenceRaw);
}

void drawTempPage() {
  drawTitle("TEMP", 7);

  display.setCursor(0, 11);
  display.print("TEMP ");
  display.print(temperatureC, 1);
  display.print("C");

  display.setCursor(78, 11);
  display.print("D");
  display.print(tempDigital ? "1" : "0");

  display.setCursor(0, 19);
  display.print("RAW ");
  display.print(tempRaw);

  display.setCursor(70, 19);
  display.print("MIN ");
  display.print(tempRawMin);
}

void drawDisplay() {
  if (!oledAddress) return;

  switch (currentPage) {
    case PAGE_HOME:
      drawHome();
      break;

    case PAGE_LIGHT:
      drawLightPage();
      break;

    case PAGE_OUTPUTS:
      drawOutputsPage();
      break;

    case PAGE_BATTERY:
      drawBatteryPage();
      break;

    case PAGE_CLOCK:
      drawClockPage();
      break;

    case PAGE_SETTINGS:
      drawSettingsPage();
      break;

    case PAGE_TEMP:
      drawTempPage();
      break;

    default:
      currentPage = PAGE_HOME;
      drawHome();
      break;
  }

  display.display();
}

// -------------------- Menu / joystick --------------------

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

  if (centred) {
    joystickReady = true;
    return;
  }

  if (!joystickReady) return;

  // Vertical = page navigation.
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

  // Horizontal = context-sensitive adjustment.
  if (abs(dx) >= abs(dy)) {

    // Light threshold.
    if (currentPage == PAGE_LIGHT || currentPage == PAGE_SETTINGS) {
      if (dx < -JOY_DEADZONE) {
        darkThreshold = max(100, darkThreshold - 50);

        Serial.print("Light threshold=");
        Serial.println(darkThreshold);

        joystickReady = false;
        return;
      }

      if (dx > JOY_DEADZONE) {
        darkThreshold = min(4000, darkThreshold + 50);

        Serial.print("Light threshold=");
        Serial.println(darkThreshold);

        joystickReady = false;
        return;
      }
    }

    // Clock adjustment.
    // On CLOCK page, left/right changes the selected field.
    if (currentPage == PAGE_CLOCK) {
      if (dx < -JOY_DEADZONE) {
        adjustClock(-1);
        joystickReady = false;
        return;
      }

      if (dx > JOY_DEADZONE) {
        adjustClock(1);
        joystickReady = false;
        return;
      }
    }

    // Temperature calibration.
    // On TEMP page, left/right shifts the RAW reference.
    // This lets us compensate for a different sensor/module without
    // changing the actual temperature slope.
    if (currentPage == PAGE_TEMP) {
      if (dx < -JOY_DEADZONE) {
        tempReferenceRaw = max(0, tempReferenceRaw - 5);

        Serial.print("Temp reference RAW=");
        Serial.println(tempReferenceRaw);

        joystickReady = false;
        return;
      }

      if (dx > JOY_DEADZONE) {
        tempReferenceRaw = min(4095, tempReferenceRaw + 5);

        Serial.print("Temp reference RAW=");
        Serial.println(tempReferenceRaw);

        joystickReady = false;
        return;
      }
    }
  }
}

void handleJoystickButton() {
  bool button = digitalRead(JOY_BTN);
  unsigned long now = millis();

  if (button == LOW &&
      lastButtonState == HIGH &&
      now - lastButtonTime > BUTTON_DEBOUNCE) {

    lastButtonTime = now;

    // CLOCK page: button cycles the editable field.
    if (currentPage == PAGE_CLOCK) {
      nextClockField();
    }

    // OUTPUTS page: cycle garden AUTO -> forced ON -> forced OFF.
    else if (currentPage == PAGE_OUTPUTS) {

      if (gardenMode == MODE_AUTO) {
        gardenMode = MODE_FORCE_ON;
      } else if (gardenMode == MODE_FORCE_ON) {
        gardenMode = MODE_FORCE_OFF;
      } else {
        gardenMode = MODE_AUTO;
      }

      updateOutputs();

      Serial.print("Garden mode=");
      Serial.println(modeName());
    }

    // TEMP page: reset observed min/max.
    else if (currentPage == PAGE_TEMP) {
      tempRawMin = tempRaw;
      tempRawMax = tempRaw;

      Serial.println("Temperature min/max reset.");
    }
  }

  lastButtonState = button;
}

// -------------------- Serial diagnostics --------------------

void printDiagnostics() {
  int x = analogRead(JOY_X);
  int y = analogRead(JOY_Y);
  int button = digitalRead(JOY_BTN);

  char clockText[16];
  printClock(clockText, sizeof(clockText));

  Serial.print("TIME=");
  Serial.print(clockText);

  float batteryVoltage = readBatteryVoltage();
  Serial.print("  BAT=");
  if (isnan(batteryVoltage)) Serial.print("N/S");
  else Serial.print(batteryVoltage, 2);
  Serial.print("V");

  Serial.print("  LIGHT=");
  Serial.print(lightRaw);

  Serial.print("  TEMP_A0=");
  Serial.print(tempRaw);

  Serial.print("  TEMP_C=");
  Serial.print(temperatureC, 1);

  Serial.print("  TEMP_D0=");
  Serial.print(tempDigital ? "HIGH" : "LOW");

  Serial.print("  TREF_RAW=");
  Serial.print(tempReferenceRaw);

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

  Serial.print("  IRRIG=");
  Serial.print(irrigation ? "ON" : "OFF");

  Serial.print("  MODE=");
  Serial.println(modeName());
}

// -------------------- Setup --------------------

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("================================");
  Serial.println(" ESP32 GARDEN CONTROLLER");
  Serial.println(" Field-test build");
  Serial.println("================================");

  pinMode(GARDEN_RELAY, OUTPUT);
  pinMode(IRRIGATION_RELAY, OUTPUT);

  // Safe startup state.
  setGardenRelay(false);
  setIrrigationRelay(false);

  pinMode(JOY_BTN, INPUT_PULLUP);

  pinMode(JOY_X, INPUT);
  pinMode(JOY_Y, INPUT);

  pinMode(LIGHT_PIN, INPUT);

  pinMode(TEMP_A0, INPUT);
  pinMode(TEMP_D0, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_SENSE_PIN, ADC_11db);

  preferences.begin("garden", false);

  // Namibia / Central Africa Time = UTC+2, no daylight-saving changes.
  setenv("TZ", "CAT-2", 1);
  tzset();
  loadClock();

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

  // Seed temperature filter so startup does not briefly display nonsense.
  int initialTemp = analogRead(TEMP_A0);
  for (int i = 0; i < TEMP_SAMPLES; i++) {
    tempSamples[i] = initialTemp;
  }

  tempRaw = initialTemp;
  tempRawMin = initialTemp;
  tempRawMax = initialTemp;
  tempSampleIndex = 0;
  tempSamplesReady = true;
  temperatureC = calculateTemperature(initialTemp);
  tempDigital = digitalRead(TEMP_D0);

  Serial.print("Joystick centre X=");
  Serial.print(joyCenterX);
  Serial.print(" Y=");
  Serial.println(joyCenterY);

  Serial.println("Known-good joystick pins: X32 Y33 BTN25");
  Serial.println("Light sensor: GPIO34");
  Serial.println("KY-028 A0: GPIO35");
  Serial.println("KY-028 D0: GPIO39");
  Serial.println("Battery monitor: GPIO36, external 100K/100K divider");
  Serial.println("Clock: software clock, saved in NVS while powered");
  Serial.println("Irrigation control: LOCKED OFF");

  readSensors();
  drawDisplay();

  Serial.println("Controller ready.");
}

// -------------------- Main loop --------------------

void loop() {
  unsigned long now = millis();

  handleJoystick();
  handleJoystickButton();

  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readSensors();
  }

  if (clockSet && now - lastClockSave >= CLOCK_SAVE_INTERVAL) {
    saveClock();
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

// -------------------- Mode text --------------------

const char *modeName() {
  switch (gardenMode) {
    case MODE_AUTO:
      return "AUTO";

    case MODE_FORCE_ON:
      return "ON";

    case MODE_FORCE_OFF:
      return "OFF";
  }

  return "?";
}
