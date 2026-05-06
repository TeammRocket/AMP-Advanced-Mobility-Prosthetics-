#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <algorithm> // For std::sort
#include <WiFiManager.h> // Handles Wi-Fi connection automatically
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// --- PIN CONFIGURATION ---
const int EMG_QUADRO_PIN = 4;
const int EMG_TWOHEAD_PIN = 5;
const int SERVO_PIN = 1;

// --- SERVO LIMITS ---
const int MIN_ANGLE = 5;   
const int MAX_ANGLE = 175; 

// --- DYNAMIC THRESHOLD VALUES ---
int THRESHOLD_QUADRO = 1500; 
int THRESHOLD_TWOHEAD = 1500;

// --- CALIBRATION SETTINGS ---
const int CALIBRATION_SAMPLES = 200; 
const float SENSITIVITY = 0.40;      

// --- EMA FILTER SETTINGS ---
const float EMA_ALPHA = 0.2; 
float smoothedQuadro = 0;
float smoothedTwohead = 0;

// --- GLOBAL VARIABLES FOR CONTROL ---
int quadroValue = 0;
int twoheadValue = 0;
bool quadro = false;
bool twohead = false;
int currentLegPosition = 90;
String legState = "REST (Fixed)";

Servo legServo;

// --- TIMERS ---
unsigned long previousMillis = 0;
const long updateInterval = 15;

unsigned long previousSerialMillis = 0;
const long serialInterval = 500;

unsigned long lastWsSendTime = 0;
const long wsInterval = 50; // Send data to Web UI every 50ms

// --- WEB & TEST MODE VARIABLES ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
bool isTestMode = false;
unsigned long simStartTime = 0;

// --- FORWARD DECLARATIONS ---
void calibrateSensors();

// --- WEBSOCKET EVENT HANDLER ---
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String msg = (char*)data;
    
    // Command: Start Calibration
    if (msg.indexOf("\"command\":\"calibrate\"") > 0) {
      Serial.println("\n[WEB COMMAND] Starting Calibration...");
      calibrateSensors();
      ws.textAll("{\"type\":\"calib_done\"}");
    } 
    // Command: Toggle Test Mode
    else if (msg.indexOf("\"command\":\"test_mode\"") > 0) {
      isTestMode = msg.indexOf("\"state\":true") > 0;
      simStartTime = millis();
      Serial.printf("\n[WEB COMMAND] Test Mode: %s\n", isTestMode ? "ON" : "OFF");
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }
}

// --- SMART CALIBRATION ROUTINE ---
void calibrateSensors() {
  int qRest[CALIBRATION_SAMPLES];
  int tRest[CALIBRATION_SAMPLES];
  int qFlex[CALIBRATION_SAMPLES];
  int tFlex[CALIBRATION_SAMPLES];

  Serial.println("\n==================================");
  Serial.println("   SMART CALIBRATION STARTING     ");
  Serial.println("==================================");
  
  Serial.println("STEP 1: RELAX your leg completely.");
  Serial.println("Recording starts in 3 seconds...");
  delay(3000);
  Serial.println("--> RECORDING REST BASELINE...");
  
  for(int i = 0; i < CALIBRATION_SAMPLES; i++) {
    qRest[i] = analogRead(EMG_QUADRO_PIN);
    tRest[i] = analogRead(EMG_TWOHEAD_PIN);
    delay(10); 
  }

  Serial.println("\nSTEP 2: Prepare to FLEX your muscles to the MAXIMUM!");
  Serial.println("Flex in 3..."); delay(1000);
  Serial.println("2..."); delay(1000);
  Serial.println("1..."); delay(1000);
  Serial.println("--> FLEX NOW! HOLD IT! RECORDING MAX PEAK...");
  
  for(int i = 0; i < CALIBRATION_SAMPLES; i++) {
    qFlex[i] = analogRead(EMG_QUADRO_PIN);
    tFlex[i] = analogRead(EMG_TWOHEAD_PIN);
    delay(10);
  }

  Serial.println("\n--> PROCESSING DATA...");

  std::sort(qRest, qRest + CALIBRATION_SAMPLES);
  std::sort(tRest, tRest + CALIBRATION_SAMPLES);
  std::sort(qFlex, qFlex + CALIBRATION_SAMPLES);
  std::sort(tFlex, tFlex + CALIBRATION_SAMPLES);

  long qRestSum = 0, tRestSum = 0;
  int restCount = 0;
  for(int i = 20; i < CALIBRATION_SAMPLES - 20; i++) {
    qRestSum += qRest[i];
    tRestSum += tRest[i];
    restCount++;
  }
  int qRestAvg = qRestSum / restCount;
  int tRestAvg = tRestSum / restCount;

  long qFlexSum = 0, tFlexSum = 0;
  for(int i = CALIBRATION_SAMPLES - 10; i < CALIBRATION_SAMPLES; i++) {
    qFlexSum += qFlex[i];
    tFlexSum += tFlex[i];
  }
  int qFlexAvg = qFlexSum / 10;
  int tFlexAvg = tFlexSum / 10;

  THRESHOLD_QUADRO = qRestAvg + ((qFlexAvg - qRestAvg) * SENSITIVITY);
  THRESHOLD_TWOHEAD = tRestAvg + ((tFlexAvg - tRestAvg) * SENSITIVITY);

  smoothedQuadro = qRestAvg;
  smoothedTwohead = tRestAvg;

  Serial.println("\n==================================");
  Serial.println("       CALIBRATION RESULTS        ");
  Serial.println("==================================");
  Serial.printf("QUADRO  -> Rest: %d | Max: %d | SET THRESHOLD: %d\n", qRestAvg, qFlexAvg, THRESHOLD_QUADRO);
  Serial.printf("TWOHEAD -> Rest: %d | Max: %d | SET THRESHOLD: %d\n", tRestAvg, tFlexAvg, THRESHOLD_TWOHEAD);
  Serial.println("==================================");
  Serial.println("Starting main control loop...\n");
}

// --- SETUP FUNCTION ---
void setup() {
  Serial.begin(115200);
  delay(1000); // Allow serial to initialize
  
  // 1. Initialize File System
  if(!LittleFS.begin(true)){
    Serial.println("LittleFS Mount Failed. Formatting...");
    return;
  }
  Serial.println("LittleFS Mounted Successfully.");

  // 2. Initialize Wi-Fi using WiFiManager (Blocks until connected)
  WiFiManager wm;
  // wm.resetSettings(); // Uncomment to force Wi-Fi setup every time
  Serial.println("Connecting to Wi-Fi (or starting AP 'Prosthesis_AP')...");
  bool res = wm.autoConnect("Prosthesis_AP");
  if(!res) {
      Serial.println("Failed to connect to Wi-Fi. Restarting...");
      ESP.restart();
  }
  Serial.print("Wi-Fi Connected! IP Address: ");
  Serial.println(WiFi.localIP());

  // 3. Initialize Servo (З обов'язковими таймерами для стабільності ESP32-S3)
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  legServo.setPeriodHertz(50); // Стандартна частота для сервоприводів
  legServo.attach(SERVO_PIN);
  legServo.write(currentLegPosition);

  // 4. Run Hardware Calibration
  calibrateSensors();

  // 5. Setup Web Server Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
  Serial.println("Web Server Started.");
}

// --- SENSOR DATA PROCESSING ---
void processEMG() {
  int rawQ = 0;
  int rawT = 0;

  if (isTestMode) {
    // Generate artificial sine waves for Web UI testing
    float timeSec = (millis() - simStartTime) / 1000.0;
    float waveQ = (sin(timeSec * 2.0) + 1.0) / 2.0; 
    float waveT = (cos(timeSec * 2.0) + 1.0) / 2.0; 
    
    // Simulate raw values dynamically going above thresholds
    rawQ = (int)(waveQ * (THRESHOLD_QUADRO * 1.5)); 
    rawT = (int)(waveT * (THRESHOLD_TWOHEAD * 1.5));
  } else {
    // Read real hardware data
    rawQ = analogRead(EMG_QUADRO_PIN);
    rawT = analogRead(EMG_TWOHEAD_PIN);
  }

  // Apply EMA filter
  smoothedQuadro = (EMA_ALPHA * rawQ) + ((1.0 - EMA_ALPHA) * smoothedQuadro);
  smoothedTwohead = (EMA_ALPHA * rawT) + ((1.0 - EMA_ALPHA) * smoothedTwohead);

  quadroValue = (int)smoothedQuadro;
  twoheadValue = (int)smoothedTwohead;

  quadro = (quadroValue > THRESHOLD_QUADRO);
  twohead = (twoheadValue > THRESHOLD_TWOHEAD);
}

// --- SMOOTH POSITION CALCULATION & SERVO CONTROL ---
void calculateSmoothPosition() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= updateInterval) {
    previousMillis = currentMillis;
    bool positionChanged = false;

    // Update global state string for Web UI and Console
    legState = "REST";

    if (quadro == true && twohead == false) {
      legState = "EXTENDING";
      if (currentLegPosition < MAX_ANGLE) {
        currentLegPosition++;
        positionChanged = true;
      }
    } 
    else if (quadro == false && twohead == true) {
      legState = "FLEXING";
      if (currentLegPosition > MIN_ANGLE) {
        currentLegPosition--;
        positionChanged = true;
      }
    }

    if (positionChanged) {
      legServo.write(currentLegPosition);
    }
  }
}

// --- MAIN LOOP ---
void loop() {
  ws.cleanupClients(); 

  processEMG();
  calculateSmoothPosition();

  unsigned long currentMillis = millis();

  // 1. Send data to WebSocket clients (Fast: every 50ms)
  if (currentMillis - lastWsSendTime >= wsInterval) {
    lastWsSendTime = currentMillis;
    
    // Create average threshold for simple UI display
    int avgThreshold = (THRESHOLD_QUADRO + THRESHOLD_TWOHEAD) / 2;

    String json = "{\"type\":\"data\",\"emgQ\":" + String(quadroValue) + 
                  ",\"emgT\":" + String(twoheadValue) +
                  ",\"threshold\":" + String(avgThreshold) + 
                  ",\"state\":\"" + legState + "\"}";
    ws.textAll(json);
  }

  // 2. Debug output to Serial Monitor (Slow: every 500ms)
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