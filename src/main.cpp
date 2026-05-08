#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ESP32Servo.h>

// --- PIN CONFIGURATION ---
const int PIN_SERVO = 2; // Servo motor pin on the receiver board

Servo myServo;

typedef struct struct_message {
  int angle;
  bool isActive;
} struct_message;

struct_message receivedData;
int currentAngle = 90;

// --- POWER SAVING VARIABLES ---
bool isServoAttached = false;
unsigned long lastMoveTime = 0;
const unsigned long SERVO_TIMEOUT = 500; // Auto-detach after 500ms

// --- ESP-NOW CALLBACK ---
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  
  if (currentAngle != receivedData.angle) {
    currentAngle = receivedData.angle;
    
    if (!isServoAttached) {
      myServo.attach(PIN_SERVO, 500, 2400);
      isServoAttached = true;
    }
    
    myServo.write(currentAngle);
    lastMoveTime = millis();
    
    Serial.printf("Moving to Angle: %d\n", currentAngle);
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myServo.setPeriodHertz(50);
  
  // Test attach and immediately put to sleep
  myServo.attach(PIN_SERVO, 500, 2400);
  myServo.write(currentAngle);
  delay(500); 
  myServo.detach(); 

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }
  Serial.println("ESP-NOW Receiver Ready.");

  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // Save power by detaching the servo when not actively moving
  if (isServoAttached && (millis() - lastMoveTime > SERVO_TIMEOUT)) {
    myServo.detach();
    isServoAttached = false;
    Serial.println("Servo Detached (Power Saving Mode)");
  }
  
  delay(10); 
}