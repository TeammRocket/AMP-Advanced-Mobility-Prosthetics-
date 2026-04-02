#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESP32Servo.h>
#include <esp_now.h>

// ESP32-S3 Pins
const int PIN_EMG = 4;    
const int PIN_SERVO = 2;

// Working Variables
Servo myServo;
int baseline = 0;
float gain = 1.0;
int threshold = 200;
float smoothedSignal = 0;
const float alpha = 0.2;

bool isTestMode = false;
unsigned long simStartTime = 0;

bool isCalibrating = false;
unsigned long calibStartTime = 0;
long calibSum = 0;
int calibCount = 0;

// Server and WebSockets
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
unsigned long lastSendTime = 0;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct struct_message {
  int angle;
  bool isActive;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Function from wifi_setup.cpp
void connectWiFi();

// Handle Web Commands
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String msg = (char*)data;
    
    if (msg.indexOf("\"command\":\"calibrate\"") > 0) {
      isCalibrating = true;
      calibStartTime = millis();
      calibSum = 0;
      calibCount = 0;
    } 
    else if (msg.indexOf("\"command\":\"test_mode\"") > 0) {
      isTestMode = msg.indexOf("\"state\":true") > 0;
      simStartTime = millis();
    }
    else if (msg.indexOf("\"command\":\"settings\"") > 0) {
      int gIdx = msg.indexOf("\"gain\":") + 7;
      int gEnd = msg.indexOf(",", gIdx);
      gain = msg.substring(gIdx, gEnd).toFloat();
      
      int tIdx = msg.indexOf("\"threshold\":") + 12;
      int tEnd = msg.indexOf("}", tIdx);
      threshold = msg.substring(tIdx, tEnd).toInt();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }
}

void setup() {
  Serial.begin(115200);
  delay(4000);
  pinMode(PIN_EMG, INPUT);
  
  // Configure Servo for S3
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myServo.setPeriodHertz(50);
  myServo.attach(PIN_SERVO, 500, 2400);
  myServo.write(0);

  // 1. File System
  if(!LittleFS.begin(true)){
    Serial.println("File System Error?");
    return;
  }

  // 2. Wi-Fi Manager
  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {    
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(WiFi.softAPIP());
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Error");
    return;
  }
  
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add ESP-NOW peer");
    return;
  }
  Serial.println("ESP-NOW Ready (Broadcast Mode)");

  // 3. Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
  Serial.println("Server running");
}

void loop() {
  ws.cleanupClients(); 

  // calibration process
  if (isCalibrating) {
    if (millis() - calibStartTime < 3000) { 
      calibSum += analogRead(PIN_EMG);
      calibCount++;
      delay(5);
    } else {
      // Правка: Захист від ділення на нуль
      if (calibCount > 0) {
        baseline = calibSum / calibCount;
      }
      isCalibrating = false;
      
      String calibMsg = "{\"type\":\"calib_done\", \"baseline\":" + String(baseline) + "}";
      ws.textAll(calibMsg);
    }
    return;
  }

  int finalSignal = 0;

  if (isTestMode) {
    float timeSec = (millis() - simStartTime) / 1000.0;
    float wave = (sin(timeSec * 2.0) + 1.0) / 2.0; 
    finalSignal = (int)(wave * 1500); 
  } else {
    // EMG signal processing
    int rawValue = analogRead(PIN_EMG);
    int noiseRemoved = abs(rawValue - baseline);
    smoothedSignal = (alpha * noiseRemoved) + ((1.0 - alpha) * smoothedSignal);
    finalSignal = (int)(smoothedSignal * gain);
  }

  // Control Servo
  bool isExtending = false;
  int targetAngle = 0;
  
  if (finalSignal > threshold) {
    targetAngle = 120;
    isExtending = true;
  } else {
    targetAngle = 0;
  }
  
  myServo.write(targetAngle);

  // --- SEND TO GRAPH (website) ---
  if (millis() - lastSendTime > 50) {
    
    String json = "{\"type\":\"data\",\"emg\":" + String(finalSignal) + 
                  ",\"threshold\":" + String(threshold) + 
                  ",\"active\":" + String(isExtending ? "true" : "false") + "}";
    ws.textAll(json);

    myData.angle = targetAngle;
    myData.isActive = isExtending;
    esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

    lastSendTime = millis();
  }
}