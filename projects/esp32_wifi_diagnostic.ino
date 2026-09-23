/*
 * ESP32 Wi-Fi AP DIAGNOSTIC
 *
 * Purpose:
 *   Test ONLY the ESP32 Wi-Fi radio as a visible 2.4 GHz access point.
 *
 * No OLED.
 * No joystick.
 * No web server.
 * No game code.
 *
 * Expected:
 *   SSID: ESP32_WIFI_TEST
 *   Password: test12345
 *   IP: 192.168.4.1
 *
 * Upload at 115200 baud if higher upload speeds are unreliable.
 */

#include <WiFi.h>

const char* TEST_SSID = "ESP32_WIFI_TEST";
const char* TEST_PASSWORD = "test12345";

void printStatus() {
  Serial.println();
  Serial.println("========== ESP32 WIFI DIAGNOSTIC ==========");
  Serial.print("WiFi mode:        ");
  Serial.println(WiFi.getMode());
  Serial.print("AP started:       ");
  Serial.println(WiFi.softAPgetStationNum() >= 0 ? "YES" : "UNKNOWN");
  Serial.print("SSID:             ");
  Serial.println(WiFi.softAPSSID());
  Serial.print("IP:               ");
  Serial.println(WiFi.softAPIP());
  Serial.print("MAC:              ");
  Serial.println(WiFi.softAPmacAddress());
  Serial.print("Channel:          ");
  Serial.println(WiFi.channel());
  Serial.print("Stations:         ");
  Serial.println(WiFi.softAPgetStationNum());
  Serial.print("TX power:         ");
  Serial.println(WiFi.getTxPower());
  Serial.println("===========================================");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("###########################################");
  Serial.println("#       ESP32 WIFI RADIO DIAGNOSTIC       #");
  Serial.println("###########################################");
  Serial.println();
  Serial.println("Resetting Wi-Fi...");

  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(500);

  Serial.println("Starting AP...");
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

  Serial.print("SSID broadcast:   YES");
  Serial.println();
  Serial.print("Requested SSID:   ");
  Serial.println(TEST_SSID);
  Serial.print("Active SSID:      ");
  Serial.println(WiFi.softAPSSID());
  Serial.print("Password:         ");
  Serial.println(TEST_PASSWORD);

  printStatus();

  Serial.println();
  Serial.println(">>> NOW SCAN FOR: ESP32_WIFI_TEST <<<");
  Serial.println(">>> Password: test12345              <<<");
  Serial.println(">>> Expected IP: 192.168.4.1         <<<");
  Serial.println();

  if (started) {
    Serial.println("AP appears to have started.");
    Serial.println("If NO phone/PC can see ESP32_WIFI_TEST,");
    Serial.println("this test is below the game/web software.");
  } else {
    Serial.println("AP FAILED TO START.");
  }
}

void loop() {
  static unsigned long lastReport = 0;

  if (millis() - lastReport >= 5000) {
    lastReport = millis();

    Serial.println();
    Serial.println("--- LIVE WIFI RADIO STATUS ---");
    Serial.print("AP SSID:    ");
    Serial.println(WiFi.softAPSSID());
    Serial.print("AP IP:      ");
    Serial.println(WiFi.softAPIP());
    Serial.print("Channel:    ");
    Serial.println(WiFi.channel());
    Serial.print("Stations:   ");
    Serial.println(WiFi.softAPgetStationNum());
    Serial.print("WiFi mode:  ");
    Serial.println(WiFi.getMode());
    Serial.println("------------------------------");
  }

  delay(10);
}
