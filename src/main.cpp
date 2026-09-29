/*
 * 
 * MIT License
 * Copyright (c) 2026 TeammRocket
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "wifi_setup.h"
#include "calibration.h"
#include "esp_now_handler.h"
#include "dsp_filters.h"

// Hardware pins
const int EMG_QUADRO_PIN = 4;
const int EMG_TWOHEAD_PIN = 1;
const int SERVO_PIN = 2;

// Servo limits
int MIN_ANGLE = 5;   
int MAX_ANGLE = 175; 
int REST_ANGLE = 90; 

// Dynamic threshold values (managed by calibration)
int THRESHOLD_QUADRO = 1500; 
int THRESHOLD_TWOHEAD = 1500;
float emgGain = 1.0f;

// Processed filter variables
float smoothedQuadro = 0;
float smoothedTwohead = 0;
const float DEADBAND_RATIO = 0.15f;

// Control variables
int quadroValue = 0;
int twoheadValue = 0;
bool quadro = false;
bool twohead = false;
int currentLegPosition = 90;
String legState = "REST (Fixed)";

Servo legServo;

// Power saving
bool isServoAttached = true;
unsigned long lastMoveTime = 0;
const unsigned long SERVO_TIMEOUT = 5000;

// Timers
unsigned long previousMillis = 0;
const long updateInterval = 15;

unsigned long previousSerialMillis = 0;
const long serialInterval = 500;

unsigned long lastWsSendTime = 0;
const long wsInterval = 50; 

// Web, ESP-NOW & Test mode variables
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool isTestMode = false;
bool useEspNow = false;
unsigned long simStartTime = 0;
bool startCalibrationFlag = false;

unsigned long lastSyncTime = 0;

// Handle WebSocket commands
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String msg = (char*)data;
    
    if (msg.indexOf("\"command\":\"calibrate\"") > 0) {
      Serial.println("\n[WEB COMMAND] Calibration Requested...");
      startCalibrationFlag = true; 
    } 
    else if (msg.indexOf("\"command\":\"test_mode\"") > 0) {
      isTestMode = msg.indexOf("\"state\":true") > 0;
      simStartTime = millis();
      Serial.printf("\n[WEB COMMAND] Test Mode: %s\n", isTestMode ? "ON" : "OFF");
    }
    else if (msg.indexOf("\"command\":\"esp_now\"") > 0) {
      useEspNow = msg.indexOf("\"state\":true") > 0;
      Serial.printf("\n[WEB COMMAND] ESP-NOW: %s\n", useEspNow ? "ON" : "OFF");
    }
    else if (msg.indexOf("\"command\":\"settings\"") > 0) {
      if (msg.indexOf("\"gain\":") > 0) {
        emgGain = msg.substring(msg.indexOf("\"gain\":") + 7, msg.indexOf(",", msg.indexOf("\"gain\":"))).toFloat();
      }
      if (msg.indexOf("\"threshold\":") > 0) {
        int t = msg.substring(msg.indexOf("\"threshold\":") + 12, msg.indexOf(",", msg.indexOf("\"threshold\":"))).toInt();
        THRESHOLD_QUADRO = t; 
        THRESHOLD_TWOHEAD = t;
      }
      if (msg.indexOf("\"restAngle\":") > 0) {
        REST_ANGLE = msg.substring(msg.indexOf("\"restAngle\":") + 12, msg.indexOf(",", msg.indexOf("\"restAngle\":"))).toInt();
      }
      if (msg.indexOf("\"minAngle\":") > 0) {
        MIN_ANGLE = msg.substring(msg.indexOf("\"minAngle\":") + 11, msg.indexOf(",", msg.indexOf("\"minAngle\":"))).toInt();
      }
      if (msg.indexOf("\"maxAngle\":") > 0) {
        MAX_ANGLE = msg.substring(msg.indexOf("\"maxAngle\":") + 11, msg.indexOf("}", msg.indexOf("\"maxAngle\":"))).toInt();
      }
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }
}

// Sensor data processing
void processEMG() {
  float rawQ = 0;
  float rawT = 0;

  if (isTestMode) {
    unsigned long elapsed = millis() - simStartTime;
    int phase = (elapsed / 2000) % 3;
    
    int noise = random(0, 50);
    int activeSignal = 500 + random(-50, 50);

    if (phase == 0) {
      rawQ = noise;
      rawT = THRESHOLD_TWOHEAD + activeSignal;
    } else if (phase == 1) {
      rawQ = noise;
      rawT = noise;
    } else if (phase == 2) {
      rawQ = THRESHOLD_QUADRO + activeSignal;
      rawT = noise;
    }
    
    // In test mode bypass filters
    smoothedQuadro = rawQ;
    smoothedTwohead = rawT;
  } else {
    rawQ = (float)analogRead(EMG_QUADRO_PIN) * emgGain;
    rawT = (float)analogRead(EMG_TWOHEAD_PIN) * emgGain;
    
    // Apply Priority 1 & 2 Filters
    smoothedQuadro = processEMGChannelQ(rawQ);
    smoothedTwohead = processEMGChannelT(rawT);
  }

  quadroValue = (int)smoothedQuadro;
  twoheadValue = (int)smoothedTwohead;

  // Priority 1: Deadband / Noise Gate
  if (quadroValue < THRESHOLD_QUADRO * DEADBAND_RATIO) quadroValue = 0;
  if (twoheadValue < THRESHOLD_TWOHEAD * DEADBAND_RATIO) twoheadValue = 0;

  // Priority 1: Crosstalk Lockout (Antagonist Invalidation)
  if (quadroValue > 0 && twoheadValue > 0) {
      if (quadroValue > twoheadValue * 1.3f) {
          twoheadValue = 0; // Quadro dominates
      } else if (twoheadValue > quadroValue * 1.3f) {
          quadroValue = 0; // Twohead dominates
      } else {
          // Unclear dominance, zero both
          quadroValue = 0;
          twoheadValue = 0;
      }
  }

  quadro = (quadroValue > THRESHOLD_QUADRO);
  twohead = (twoheadValue > THRESHOLD_TWOHEAD);
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  
  // 1. File system
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  // 2. Wi-Fi
  connectWiFi();

  // 3. ESP-NOW
  initEspNow();

  // Initialize DSP Filters
  initDSP();

  // 4. Servo configuration
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  legServo.setPeriodHertz(50); 
  legServo.attach(SERVO_PIN, 500, 2400);
  legServo.write(currentLegPosition);
  lastMoveTime = millis();

  // 5. Initial hardware calibration
  calibrateSensors(EMG_QUADRO_PIN, EMG_TWOHEAD_PIN);

  // 6. Web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/style.css", "text/css");
  });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/script.js", "application/javascript");
  });
  server.serveStatic("/", LittleFS, "/");

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
  Serial.println("Web Server Started.");
}

void loop() {
  ws.cleanupClients(); 

  // Calibration request from web
  if (startCalibrationFlag) {
    calibrateSensors(EMG_QUADRO_PIN, EMG_TWOHEAD_PIN);
    ws.textAll("{\"type\":\"calib_done\"}");
    startCalibrationFlag = false;
  }

  processEMG();

  // Smooth position calculation
  unsigned long currentMillis = millis();
  bool positionChanged = false;

  if (currentMillis - previousMillis >= updateInterval) {
    previousMillis = currentMillis;
    legState = "REST (Fixed)";

    if (quadro && !twohead) {
      legState = "EXTENDING";
      if (currentLegPosition < MAX_ANGLE) {
        currentLegPosition++;
        positionChanged = true;
      }
    } 
    else if (!quadro && twohead) {
      legState = "FLEXING";
      if (currentLegPosition > MIN_ANGLE) {
        currentLegPosition--;
        positionChanged = true;
      }
    }
  }

  // Actuator control & power saving
  if (!useEspNow) {
    if (positionChanged) {
      if (!isServoAttached) {
        legServo.attach(SERVO_PIN, 500, 2400);
        isServoAttached = true;
      }
      legServo.write(currentLegPosition);
      lastMoveTime = currentMillis;
    } else {
      if (isServoAttached && (currentMillis - lastMoveTime > SERVO_TIMEOUT)) {
        legServo.detach();
        isServoAttached = false;
      }
    }
  } else {
    if (isServoAttached) {
      legServo.detach();
      isServoAttached = false;
    }
    // Receiver sync every 250ms
    if (!receiverConnected && currentMillis - lastSyncTime > 250) {
      syncEspNowReceiver();
      lastSyncTime = currentMillis;
    }
    if (receiverConnected && positionChanged) {
      sendEspNowAngle(currentLegPosition);
    }
  }

  // Web telemetry
  if (currentMillis - lastWsSendTime >= wsInterval) {
    lastWsSendTime = currentMillis;
    
    int avgThreshold = (THRESHOLD_QUADRO + THRESHOLD_TWOHEAD) / 2;

    String json = "{\"type\":\"data\",\"emgQ\":" + String(quadroValue) + 
                  ",\"emgT\":" + String(twoheadValue) +
                  ",\"threshold\":" + String(avgThreshold) + 
                  ",\"state\":\"" + legState + "\"" +
                  ",\"rxStatus\":\"" + (receiverConnected ? "Connected" : "Searching") + "\"" +
                  ",\"rxMac\":\"" + rxMacString + "\"}";
    ws.textAll(json);
  }

  // Debug output
  if (currentMillis - previousSerialMillis >= serialInterval) {
    previousSerialMillis = currentMillis;
    Serial.printf("[%s] Raw Q: %4d | Raw T: %4d | Angle: %3d | State: %s\n", 
                  isTestMode ? "TEST " : "REAL",
                  quadroValue, twoheadValue,
                  currentLegPosition, 
                  legState.c_str());
  }

  delay(10);
}