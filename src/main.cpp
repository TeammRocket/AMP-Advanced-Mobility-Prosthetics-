#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ESP32Servo.h>

const int SERVO_PIN = 2; 
Servo legServo;

typedef struct struct_message {
  uint8_t type; 
  uint8_t macAddr[6];
  int angle;
} struct_message;

struct_message incomingData;
struct_message replyData;

uint8_t senderAddress[6];
bool isConnected = false;

unsigned long lastMoveTime = 0;
const unsigned long SERVO_TIMEOUT = 5000; 
bool isServoAttached = false;
int currentAngle = 90;

unsigned long lastHopTime = 0;
int currentChannel = 1;

void OnDataRecv(const uint8_t * mac, const uint8_t *incoming, int len) {
  if (len == sizeof(struct_message)) {
    memcpy(&incomingData, incoming, sizeof(incomingData));
    
    if (incomingData.type == 0 && !isConnected) { 
      memcpy(senderAddress, incomingData.macAddr, 6);
      isConnected = true;
      
      esp_now_peer_info_t peerInfo;
      memset(&peerInfo, 0, sizeof(peerInfo)); // Fix for reliable connection
      memcpy(peerInfo.peer_addr, senderAddress, 6);
      peerInfo.channel = currentChannel;
      peerInfo.encrypt = false;
      esp_now_add_peer(&peerInfo);

      replyData.type = 1; 
      WiFi.macAddress(replyData.macAddr);
      esp_now_send(senderAddress, (uint8_t *) &replyData, sizeof(replyData));
    } 
    else if (incomingData.type == 2 && isConnected) { 
      currentAngle = incomingData.angle;
      
      if (!isServoAttached) {
        legServo.attach(SERVO_PIN, 500, 2400);
        isServoAttached = true;
      }
      legServo.write(currentAngle);
      lastMoveTime = millis();
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  ESP32PWM::allocateTimer(0);
  legServo.setPeriodHertz(50); 
  
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnDataRecv);
  }
}

void loop() {
  if (!isConnected) {
    // Slower hop (600ms) to ensure it catches the SYNC signal
    if (millis() - lastHopTime > 600) {
      currentChannel++;
      if (currentChannel > 13) currentChannel = 1;
      esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
      lastHopTime = millis();
    }
  } else {
    if (isServoAttached && (millis() - lastMoveTime > SERVO_TIMEOUT)) {
      legServo.detach();
      isServoAttached = false;
    }
  }
  delay(10);
}