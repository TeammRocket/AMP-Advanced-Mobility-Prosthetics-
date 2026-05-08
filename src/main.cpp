#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESP32Servo.h>
#include <esp_now.h>

const int PIN_EMG_QUADRO = 4;
const int PIN_EMG_TWOHEAD = 5;
const int PIN_SERVO = 1;

Servo myServo;
int currentLegPosition = 90;

// Variables for servo power saving
bool isServoAttached = true;
unsigned long lastMoveTime = 0;
const unsigned long SERVO_TIMEOUT = 500; // Time before turning off the motor (ms)

int baselineQ = 0, baselineT = 0;
float gain = 0.2;
int threshold = 1000;

float smoothedQ = 0, smoothedT = 0;
const float alpha = 0.2;

bool isTestMode = false;
bool useEspNow = false;
unsigned long simStartTime = 0;

bool isCalibrating = false;
unsigned long calibStartTime = 0;
long calibSumQ = 0, calibSumT = 0;
int calibCount = 0;

unsigned long previousMillis = 0;
const long updateInterval = 15;
unsigned long lastSendTime = 0;

// WEB & ESP-NOW
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
typedef struct struct_message {
  int angle;
  bool isActive;
} struct_message;
struct_message myData;
esp_now_peer_info_t peerInfo;

void connectWiFi();

// WEBSOCKET HANDLER
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String msg = (char*)data;
    
    if (msg.indexOf("\"command\":\"calibrate\"") > 0) {
      isCalibrating = true;
      calibStartTime = millis();
      calibSumQ = calibSumT = calibCount = 0;
    } 
    else if (msg.indexOf("\"command\":\"test_mode\"") > 0) {
      isTestMode = msg.indexOf("\"state\":true") > 0;
      simStartTime = millis();
    }
    else if (msg.indexOf("\"command\":\"esp_now\"") > 0) {
      useEspNow = msg.indexOf("\"state\":true") > 0;
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
  if (type == WS_EVT_DATA) handleWebSocketMessage(arg, data, len);
}

// SETUP
void setup() {
  Serial.begin(115200);
  pinMode(PIN_EMG_QUADRO, INPUT);
  pinMode(PIN_EMG_TWOHEAD, INPUT);
  
  // Init Servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myServo.setPeriodHertz(50);
  myServo.attach(PIN_SERVO, 500, 2400);
  myServo.write(currentLegPosition);
  lastMoveTime = millis();

  if(!LittleFS.begin(true)) Serial.println("FS Error");

  connectWiFi();

  // Init ESP-NOW
  if (esp_now_init() == ESP_OK) {
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  // Init Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
}

// MAIN LOOP
void loop() {
  ws.cleanupClients(); 

  // 1. Calibration Routine
  if (isCalibrating) {
    if (millis() - calibStartTime < 3000) { 
      calibSumQ += analogRead(PIN_EMG_QUADRO);
      calibSumT += analogRead(PIN_EMG_TWOHEAD);
      calibCount++;
      delay(5);
    } else {
      if (calibCount > 0) {
        baselineQ = calibSumQ / calibCount;
        baselineT = calibSumT / calibCount;
      }
      isCalibrating = false;
      ws.textAll("{\"type\":\"calib_done\"}");
    }
    return;
  }

  // 2. Process EMG Data
  int finalQ = 0, finalT = 0;

  if (isTestMode) {
    float timeSec = (millis() - simStartTime) / 1000.0;
    finalQ = (int)(((sin(timeSec * 3.0) + 1.0) / 2.0) * 1500); 
    finalT = (int)(((cos(timeSec * 3.0) + 1.0) / 2.0) * 1500);
  } else {
    int rawQ = abs(analogRead(PIN_EMG_QUADRO) - baselineQ);
    int rawT = abs(analogRead(PIN_EMG_TWOHEAD) - baselineT);
    
    smoothedQ = (alpha * rawQ) + ((1.0 - alpha) * smoothedQ);
    smoothedT = (alpha * rawT) + ((1.0 - alpha) * smoothedT);
    
    finalQ = (int)(smoothedQ * gain);
    finalT = (int)(smoothedT * gain);
  }

  bool quadroActive = finalQ > threshold;
  bool twoheadActive = finalT > threshold;
  String stateStr = "REST";
  bool positionChanged = false; // Flag to track if the angle changed this cycle

  // 3. Smooth Position Calculation
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= updateInterval) {
    previousMillis = currentMillis;

    if (quadroActive && !twoheadActive) {
      if (currentLegPosition < 180) {
        currentLegPosition++; // Extending
        positionChanged = true;
      }
      stateStr = "EXTENDING";
    } 
    else if (!quadroActive && twoheadActive) {
      if (currentLegPosition > 0) {
        currentLegPosition--;   // Flexing
        positionChanged = true;
      }
      stateStr = "FLEXING";
    }
  }

  // 4. Update Hardware (with power saving logic)
  if (!useEspNow) {
    if (positionChanged) {
      // Re-attach servo if it was detached
      if (!isServoAttached) {
        myServo.attach(PIN_SERVO, 500, 2400); 
        isServoAttached = true;
      }
      myServo.write(currentLegPosition);
      lastMoveTime = millis();
    } else {
      // Detach servo to save battery if inactive for SERVO_TIMEOUT ms
      if (isServoAttached && (millis() - lastMoveTime > SERVO_TIMEOUT)) {
        myServo.detach(); 
        isServoAttached = false;
      }
    }
  } else {
    // If ESP-NOW is enabled, local servo must be detached
    if (isServoAttached) {
      myServo.detach();
      isServoAttached = false;
    }
  }

  // 5. Send Data (Web UI & ESP-NOW)
  if (millis() - lastSendTime > 50) {
    // Send to Web
    String json = "{\"type\":\"data\",\"emgQ\":" + String(finalQ) + 
                  ",\"emgT\":" + String(finalT) +
                  ",\"threshold\":" + String(threshold) + 
                  ",\"state\":\"" + stateStr + "\"}";
    ws.textAll(json);

    // Send via ESP-NOW
    if (useEspNow) {
      myData.angle = currentLegPosition;
      myData.isActive = (stateStr != "REST");
      esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    }
    
    lastSendTime = millis();
  }
}