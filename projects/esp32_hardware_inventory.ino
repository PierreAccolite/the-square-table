/*
 * ESP32 HARDWARE INVENTORY / RADIO DIAGNOSTIC
 *
 * Pocket Arcade project
 *
 * Purpose:
 *   Take a software stock-take of the ESP32 board and run several
 *   independent hardware/peripheral checks before we move on to USB.
 *
 * Tests:
 *   - ESP32 chip model / revision / cores
 *   - CPU frequency
 *   - Flash / heap / PSRAM
 *   - Factory MAC addresses
 *   - ESP-IDF / Arduino core versions
 *   - GPIO / peripheral capability information
 *   - ADC sanity checks on safe-ish input pins
 *   - Wi-Fi active scan
 *   - Wi-Fi passive scan
 *   - Wi-Fi AP startup
 *   - Bluetooth presence as reported by chip capabilities
 *
 * NOTE:
 *   This sketch intentionally does NOT initialise the Pocket Arcade OLED
 *   or joystick, so it can be used as a clean board-level diagnostic.
 *
 * Upload at 115200 baud if higher upload speeds are unreliable.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include "soc/soc_caps.h"

const char* TEST_SSID = "ESP32_INVENTORY_TEST";
const char* TEST_PASSWORD = "test12345";

void printLine() {
  Serial.println("-------------------------------------------");
}

void printChipInventory() {
  esp_chip_info_t chipInfo;
  esp_chip_info(&chipInfo);

  Serial.println();
  Serial.println("========== CHIP INVENTORY ==========");

  Serial.print("Model:              ");
  Serial.println(ESP.getChipModel());

  Serial.print("Chip revision:      ");
  Serial.println(chipInfo.revision);

  Serial.print("CPU cores:          ");
  Serial.println(chipInfo.cores);

  Serial.print("CPU frequency:      ");
  Serial.print(getCpuFrequencyMhz());
  Serial.println(" MHz");

  Serial.print("Chip features:      0x");
  Serial.println(chipInfo.features, HEX);

  Serial.print("SDK / IDF:          ");
  Serial.println(ESP.getSdkVersion());

  Serial.print("Arduino core:       ");
  Serial.println(ESP_ARDUINO_VERSION_STR);

  Serial.print("Chip temperature:   ");
  Serial.print(temperatureRead());
  Serial.println(" C");

  Serial.println();
  Serial.println("Capability macros:");
  Serial.print("  Wi-Fi:            ");
  Serial.println(SOC_WIFI_SUPPORTED ? "YES" : "NO");

  Serial.print("  Bluetooth:        ");
  Serial.println(SOC_BT_SUPPORTED ? "YES" : "NO");

#ifdef SOC_BT_CLASSIC_SUPPORTED
  Serial.print("  Bluetooth Classic:");
  Serial.println(SOC_BT_CLASSIC_SUPPORTED ? " YES" : " NO");
#endif

#ifdef SOC_BLE_SUPPORTED
  Serial.print("  BLE:              ");
  Serial.println(SOC_BLE_SUPPORTED ? "YES" : "NO");
#endif

#ifdef SOC_USB_OTG_SUPPORTED
  Serial.print("  USB OTG:          ");
  Serial.println(SOC_USB_OTG_SUPPORTED ? "YES" : "NO");
#else
  Serial.println("  USB OTG:          not exposed by this core");
#endif

#ifdef SOC_TOUCH_SENSOR_SUPPORTED
  Serial.print("  Touch sensor:     ");
  Serial.println(SOC_TOUCH_SENSOR_SUPPORTED ? "YES" : "NO");
#endif

#ifdef SOC_DAC_SUPPORTED
  Serial.print("  DAC:              ");
  Serial.println(SOC_DAC_SUPPORTED ? "YES" : "NO");
#endif

#ifdef SOC_RMT_SUPPORTED
  Serial.print("  RMT:              ");
  Serial.println(SOC_RMT_SUPPORTED ? "YES" : "NO");
#endif

  Serial.print("  GPIO count:       ");
  Serial.println(SOC_GPIO_PIN_COUNT);

  Serial.println("=====================================");
}

void printMemoryInventory() {
  Serial.println();
  Serial.println("========== MEMORY INVENTORY =========");

  Serial.print("Flash chip size:    ");
  Serial.print(ESP.getFlashChipSize() / 1024);
  Serial.println(" KB");

  // getFlashChipRealSize() is not available in some Arduino-ESP32
  // core versions, so use the supported flash-size API here.
  Serial.print("Flash speed:        ");
  Serial.print(ESP.getFlashChipSpeed() / 1000000);
  Serial.println(" MHz");

  Serial.print("Heap total:         ");
  Serial.print(ESP.getHeapSize());
  Serial.println(" bytes");

  Serial.print("Heap free:          ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");

  Serial.print("Heap minimum free:  ");
  Serial.print(ESP.getMinFreeHeap());
  Serial.println(" bytes");

  Serial.print("Largest free block: ");
  Serial.print(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.println(" bytes");

  Serial.print("PSRAM:              ");
  Serial.println(psramFound() ? "PRESENT" : "NOT PRESENT");

  if (psramFound()) {
    Serial.print("PSRAM size:         ");
    Serial.print(ESP.getPsramSize());
    Serial.println(" bytes");

    Serial.print("PSRAM free:         ");
    Serial.print(ESP.getFreePsram());
    Serial.println(" bytes");
  }

  Serial.println("=====================================");
}

void printMacInventory() {
  uint8_t mac[6];

  Serial.println();
  Serial.println("========== MAC INVENTORY ============");

  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf(
    "Wi-Fi STA MAC:      %02X:%02X:%02X:%02X:%02X:%02X\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
  );

  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  Serial.printf(
    "Wi-Fi AP MAC:       %02X:%02X:%02X:%02X:%02X:%02X\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
  );

#if SOC_BT_SUPPORTED
  esp_read_mac(mac, ESP_MAC_BT);
  Serial.printf(
    "Bluetooth MAC:      %02X:%02X:%02X:%02X:%02X:%02X\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
  );
#endif

  Serial.print("Arduino MAC:        ");
  Serial.println(WiFi.macAddress());

  Serial.println("=====================================");
}

void printGPIOInventory() {
  Serial.println();
  Serial.println("========== PERIPHERAL INVENTORY =====");

  Serial.println("Known Pocket Arcade pins:");
  Serial.println("  OLED SDA:         GPIO4");
  Serial.println("  OLED SCL:         GPIO15");
  Serial.println("  OLED RESET:       GPIO16");
  Serial.println("  Joystick X:       GPIO32");
  Serial.println("  Joystick Y:       GPIO33");
  Serial.println("  Joystick button:  GPIO25");

  Serial.println();
  Serial.println("Common ESP32 capabilities:");
  Serial.println("  ADC:              available on ADC-capable GPIOs");
  Serial.println("  DAC:              GPIO25 / GPIO26 on classic ESP32");
  Serial.println("  Touch:            GPIO touch-capable pins");
  Serial.println("  SPI:              hardware peripheral");
  Serial.println("  I2C:              hardware peripheral");
  Serial.println("  UART:             hardware peripheral");

  Serial.println("=====================================");
}

void runADCCheck() {
  // These are the existing joystick pins, so this test is informational only.
  // No pin modes are changed beyond INPUT.
  Serial.println();
  Serial.println("========== ADC SANITY CHECK =========");

  pinMode(32, INPUT);
  pinMode(33, INPUT);

  int x = analogRead(32);
  int y = analogRead(33);

  Serial.print("GPIO32 ADC:         ");
  Serial.println(x);

  Serial.print("GPIO33 ADC:         ");
  Serial.println(y);

  Serial.println("Move the joystick and repeat manually if desired.");
  Serial.println("=====================================");
}

void runActiveScan() {
  Serial.println();
  Serial.println("========== WI-FI ACTIVE SCAN =========");
  Serial.println("Scanning nearby 2.4 GHz networks...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(300);

  int count = WiFi.scanNetworks(false, true, false);

  Serial.print("Active scan result: ");
  Serial.println(count);

  if (count > 0) {
    Serial.println(" #   RSSI     CH   SSID");
    Serial.println("-------------------------------------------");

    for (int i = 0; i < count; i++) {
      Serial.printf(
        "%2d  %5d dBm  %2d   %s\n",
        i + 1,
        WiFi.RSSI(i),
        WiFi.channel(i),
        WiFi.SSID(i).c_str()
      );
    }
  } else if (count == 0) {
    Serial.println("ZERO NETWORKS FOUND.");
  } else {
    Serial.println("SCAN ERROR.");
  }

  WiFi.scanDelete();
  Serial.println("======================================");
}

void runPassiveScan() {
  Serial.println();
  Serial.println("========== WI-FI PASSIVE SCAN ========");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(300);

  Serial.println("Passive scan: listen for beacons only.");
  Serial.println("This does not rely on active probe requests.");

  int count = WiFi.scanNetworks(false, true, true, 300);

  Serial.print("Passive scan result: ");
  Serial.println(count);

  if (count > 0) {
    Serial.println(" #   RSSI     CH   SSID");
    Serial.println("-------------------------------------------");

    for (int i = 0; i < count; i++) {
      Serial.printf(
        "%2d  %5d dBm  %2d   %s\n",
        i + 1,
        WiFi.RSSI(i),
        WiFi.channel(i),
        WiFi.SSID(i).c_str()
      );
    }
  } else if (count == 0) {
    Serial.println("ZERO NETWORKS FOUND.");
  } else {
    Serial.println("SCAN ERROR.");
  }

  WiFi.scanDelete();
  Serial.println("======================================");
}

void runAPTest() {
  Serial.println();
  Serial.println("========== WI-FI AP TEST =============");

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(300);

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  WiFi.softAPConfig(
    IPAddress(192, 168, 4, 1),
    IPAddress(192, 168, 4, 1),
    IPAddress(255, 255, 255, 0)
  );

  bool started = WiFi.softAP(
    TEST_SSID,
    TEST_PASSWORD,
    6,
    false,
    4
  );

  Serial.print("softAP() result:  ");
  Serial.println(started ? "SUCCESS" : "FAILED");

  Serial.print("SSID:              ");
  Serial.println(WiFi.softAPSSID());

  Serial.print("IP:                ");
  Serial.println(WiFi.softAPIP());

  Serial.print("AP MAC:            ");
  Serial.println(WiFi.softAPmacAddress());

  Serial.print("Channel:            ");
  Serial.println(WiFi.channel());

  Serial.print("Stations:           ");
  Serial.println(WiFi.softAPgetStationNum());

  Serial.println("======================================");
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println("###########################################");
  Serial.println("#       ESP32 HARDWARE INVENTORY          #");
  Serial.println("#          POCKET ARCADE LAB              #");
  Serial.println("###########################################");

  printChipInventory();
  printMemoryInventory();
  printMacInventory();
  printGPIOInventory();
  runADCCheck();

  Serial.println();
  Serial.println("Now testing Wi-Fi using the Arduino driver.");
  Serial.println("Espressif documents STA scanning as a standard");
  Serial.println("ESP32 Wi-Fi capability.");

  runActiveScan();
  runPassiveScan();
  runAPTest();

  Serial.println();
  Serial.println("===========================================");
  Serial.println("INVENTORY COMPLETE");
  Serial.println("===========================================");
  Serial.println("Leave this running while checking for:");
  Serial.println(TEST_SSID);
  Serial.println("Password: test12345");
  Serial.println("===========================================");
}

void loop() {
  static unsigned long lastReport = 0;

  if (millis() - lastReport >= 5000) {
    lastReport = millis();

    Serial.println();
    Serial.println("--- LIVE STATUS ---");
    Serial.print("Heap free:    ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("CPU:          ");
    Serial.print(getCpuFrequencyMhz());
    Serial.println(" MHz");
    Serial.print("AP SSID:      ");
    Serial.println(WiFi.softAPSSID());
    Serial.print("AP IP:        ");
    Serial.println(WiFi.softAPIP());
    Serial.print("Stations:     ");
    Serial.println(WiFi.softAPgetStationNum());
    Serial.println("-------------------");
  }

  delay(10);
}
