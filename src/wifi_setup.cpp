#include "wifi_setup.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

// Separate compilation unit avoids AsyncWebServer header conflicts
void connectWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFiManager wm;
  Serial.println("[WiFi] Connecting...");

  bool res = wm.autoConnect("Prosthesis_AP");
  if (!res) {
    Serial.println("[WiFi] Connection failed. Restarting...");
    ESP.restart();
  }

  Serial.print("[WiFi] Connected! IP: ");
  Serial.println(WiFi.localIP());
}
