/*
  POCKET ARCADE - BLE RADIO FINAL DIAGNOSTIC
  ------------------------------------------
  Classic ESP32 / ESP32-D0WDQ6

  Purpose:
    Final standalone test of the ESP32 2.4 GHz radio using BLE scanning.

  This does NOT modify:
    - esp32_dev_arcade.ino
    - esp32_usb_serial_diagnostic.ino
    - esp32_bluetooth_serial_diagnostic.ino

  Test:
    1. Upload this sketch.
    2. Keep USB connected to the PC.
    3. Open Serial Monitor at 115200.
    4. Watch the BLE scan results.

  A healthy radio should normally discover nearby BLE advertisements
  from phones, watches, PCs, sensors, etc.

  The scan repeats every 10 seconds.
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>

static BLEScan* bleScan = nullptr;

static const uint32_t SCAN_SECONDS = 5;
static const uint32_t SCAN_INTERVAL_MS = 10000;

unsigned long bootTime = 0;
unsigned long lastScan = 0;
unsigned long scanNumber = 0;

void runBLEScan() {
  scanNumber++;

  Serial.println();
  Serial.println("========================================");
  Serial.print("BLE SCAN #");
  Serial.println(scanNumber);
  Serial.println("========================================");

  Serial.print("Starting ");
  Serial.print(SCAN_SECONDS);
  Serial.println("-second active BLE scan...");

  BLEScanResults* results = bleScan->start(SCAN_SECONDS, false);

  int count = results ? results->getCount() : 0;

  Serial.print("BLE devices found: ");
  Serial.println(count);

  if (count == 0) {
    Serial.println("NO BLE DEVICES FOUND.");
    Serial.println("If phones/BT devices are nearby and discoverable,");
    Serial.println("this is strong evidence of a radio-level problem.");
  }
  else {
    Serial.println();
    Serial.println("Discovered devices:");

    for (int i = 0; i < count; i++) {
      BLEAdvertisedDevice device = results->getDevice(i);

      Serial.print("  ");
      Serial.print(i + 1);
      Serial.print(": ");

      if (device.haveName()) {
        Serial.print(device.getName().c_str());
      }
      else {
        Serial.print("(no name)");
      }

      Serial.print("  address=");
      Serial.print(device.getAddress().toString().c_str());

      if (device.haveRSSI()) {
        Serial.print("  RSSI=");
        Serial.print(device.getRSSI());
        Serial.print(" dBm");
      }

      Serial.println();
    }
  }

  bleScan->clearResults();

  Serial.println("----------------------------------------");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  bootTime = millis();

  Serial.println();
  Serial.println("########################################");
  Serial.println("#   POCKET ARCADE BLE RADIO TEST       #");
  Serial.println("########################################");
  Serial.println();

  Serial.print("Chip: ");
  Serial.println(ESP.getChipModel());

  Serial.print("CPU: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");

  Serial.print("Free heap before BLE init: ");
  Serial.println(ESP.getFreeHeap());

  Serial.println();
  Serial.println("Initializing BLE...");

  BLEDevice::init("POCKET_ARCADE_BLE_TEST");

  bleScan = BLEDevice::getScan();

  if (bleScan == nullptr) {
    Serial.println("ERROR: BLE scan object unavailable.");
    Serial.println("BLE initialization failed.");
    return;
  }

  bleScan->setActiveScan(true);
  bleScan->setInterval(100);
  bleScan->setWindow(80);

  Serial.println("BLE initialized.");
  Serial.println("Active scan enabled.");
  Serial.println();
  Serial.println("READY");
  Serial.println("----------------------------------------");

  runBLEScan();
  lastScan = millis();
}

void loop() {
  if (bleScan == nullptr) {
    delay(1000);
    return;
  }

  if (millis() - lastScan >= SCAN_INTERVAL_MS) {
    lastScan = millis();
    runBLEScan();
  }

  delay(50);
}
