#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ESP32Servo.h>

const int PIN_SERVO = 2;

Servo receiverServo;

typedef struct struct_message {
  int angle;
  bool isActive;
} struct_message;

struct_message myData;

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  
  receiverServo.write(myData.angle);
}

void setup() {
  Serial.begin(115200);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  receiverServo.setPeriodHertz(50);
  receiverServo.attach(PIN_SERVO, 500, 2400);
  receiverServo.write(0);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Error");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
  
}

void loop() {
  delay(1000);
}