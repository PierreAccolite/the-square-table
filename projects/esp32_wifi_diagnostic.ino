/*
 * ESP32 WI-FI RADIO DIAGNOSTIC - RX + AP TEST
 *
 * Purpose:
 *   Test the ESP32 Wi-Fi radio independently of the Pocket Arcade.
 *
 * Test sequence:
 *   1. Reset Wi-Fi radio.
 *   2. Scan for nearby 2.4 GHz Wi-Fi networks.
 *   3. Print every network found with SSID, RSSI, channel and security.
 *   4. Start a simple ESP32 access point.
 *   5. Keep reporting AP status.
 *
 * No OLED.
 * No joystick.
 * No web server.
 * No game code.
 *
 * IMPORTANT:
 *   Classic ESP32 Wi-Fi is 2.4 GHz only.
 *
 * Upload at 115200 baud if higher upload speeds are unreliable.
 */

#include <WiFi.h>

const char* TEST_SSID = "ESP32_WIFI_TEST";
const char* TEST_PASSWORD = "test12345";

void scanNearbyNetworks() {
  Serial.println();
  Serial.println("===========================================");
  Serial.println("        WI-FI RECEIVE / SCAN TEST");
  Serial.println("===========================================");
  Serial.println("Scanning for nearby 2.4 GHz networks...");
  Serial.println();

  // Put radio into station mode for the receive test.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(300);

  int networkCount = WiFi.scanNetworks(false, true);

  if (networkCount < 0) {
    Serial.print("SCAN RESULT: ERROR ");
    Serial.println(networkCount);
    WiFi.scanDelete();
    return;
  }

  Serial.print("Networks found: ");
  Serial.println(networkCount);
  Serial.println();

  if (networkCount == 0) {
    Serial.println("NO NETWORKS FOUND.");
    Serial.println();
    Serial.println("If there are known 2.4 GHz networks nearby,");
    Serial.println("this is significant: the ESP32 radio is not");
    Serial.println("successfully receiving Wi-Fi beacons.");
  } else {
    Serial.println(" #   RSSI     CH   SECURITY   SSID");
    Serial.println("-------------------------------------------");

    for (int i = 0; i < networkCount; i++) {
      Serial.printf(
        "%2d  %5d dBm  %2d   %-9s  %s\n",
        i + 1,
        WiFi.RSSI(i),
        WiFi.channel(i),
        WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "SECURED",
        WiFi.SSID(i).c_str()
      );
    }
  }

  Serial.println();
  Serial.println("Deleting scan results...");
  WiFi.scanDelete();

  Serial.println("RX scan test complete.");
  Serial.println("===========================================");
}

void printAPStatus() {
  Serial.println();
  Serial.println("--- LIVE AP STATUS ---");
  Serial.print("AP started:  ");
  Serial.println(WiFi.softAPIP().toString() == "0.0.0.0" ? "NO" : "YES");
  Serial.print("SSID:        ");
  Serial.println(WiFi.softAPSSID());
  Serial.print("IP:          ");
  Serial.println(WiFi.softAPIP());
  Serial.print("MAC:         ");
  Serial.println(WiFi.softAPmacAddress());
  Serial.print("Channel:     ");
  Serial.println(WiFi.channel());
  Serial.print("Stations:    ");
  Serial.println(WiFi.softAPgetStationNum());
  Serial.print("WiFi mode:   ");
  Serial.println(WiFi.getMode());
  Serial.println("----------------------");
}

void startTestAP() {
  Serial.println();
  Serial.println("===========================================");
  Serial.println("          WI-FI ACCESS POINT TEST");
  Serial.println("===========================================");
  Serial.println("Starting ESP32 test AP...");

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

  Serial.println();
  Serial.print("softAP() result: ");
  Serial.println(started ? "SUCCESS" : "FAILED");

  Serial.print("Requested SSID:   ");
  Serial.println(TEST_SSID);
  Serial.print("Active SSID:      ");
  Serial.println(WiFi.softAPSSID());
  Serial.print("Password:         ");
  Serial.println(TEST_PASSWORD);
  Serial.print("AP IP:            ");
  Serial.println(WiFi.softAPIP());

  printAPStatus();

  Serial.println();
  Serial.println("===========================================");
  Serial.println("FIELD TEST");
  Serial.println("===========================================");
  Serial.println("1. Look for: ESP32_WIFI_TEST");
  Serial.println("2. Password: test12345");
  Serial.println("3. If visible, connect and report it.");
  Serial.println("4. If invisible, report the scan results above.");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("###########################################");
  Serial.println("#      ESP32 WI-FI RADIO DIAGNOSTIC      #");
  Serial.println("###########################################");
  Serial.println();

  Serial.println("Resetting Wi-Fi radio...");

  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(500);

  // FIRST: test whether the ESP32 can actually receive Wi-Fi.
  scanNearbyNetworks();

  // SECOND: test whether it can advertise an AP.
  startTestAP();
}

void loop() {
  static unsigned long lastReport = 0;

  if (millis() - lastReport >= 5000) {
    lastReport = millis();
    printAPStatus();
  }

  delay(10);
}
