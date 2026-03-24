#include <Arduino.h>
#include <WiFiManager.h>

// Initialize Wi-Fi Manager
// Separate file avoids conflicts with AsyncWebServer
void connectWiFi() {
  WiFiManager wm;
  Serial.println("Connecting to Wi-Fi...");
  
  bool res = wm.autoConnect("AMP_Prosthetic", "amp12345"); 
  
  if(!res) {
    Serial.println("Connection failed. Created AP: AMP_Prosthetic");
  } else {
    Serial.println("Wi-Fi Connected! IP: " + WiFi.localIP().toString());
  }
}