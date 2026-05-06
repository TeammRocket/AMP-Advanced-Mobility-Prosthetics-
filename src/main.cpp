#include <Arduino.h>
#include <ESP32Servo.h>

// --- PIN CONFIGURATION ---
const int EMG_QUADRO_PIN = 4;
const int EMG_TWOHEAD_PIN = 5;
const int SERVO_PIN = 18;

// --- DYNAMIC THRESHOLD VALUES (Calculated at startup) ---
int THRESHOLD_QUADRO = 1500; // Default values, will be overwritten by calibration
int THRESHOLD_TWOHEAD = 1500;

// --- CALIBRATION SETTINGS ---
const int CALIBRATION_SAMPLES = 200; // 200 samples = ~2 seconds of reading
const float SENSITIVITY = 0.40;      // 0.40 = Motor activates at 40% of max muscle strength

// --- GLOBAL VARIABLES FOR CONTROL ---
int quadroValue = 0;
int twoheadValue = 0;
bool quadro = false;
bool twohead = false;
int currentLegPosition = 90;

Servo legServo;

unsigned long previousMillis = 0;
const long updateInterval = 15;

unsigned long previousSerialMillis = 0;
const long serialInterval = 500;

// --- HELPER FUNCTION: SORT ARRAY (For finding median and peaks) ---
void sortArray(int a[], int size) {
  for(int i=0; i<(size-1); i++) {
    for(int o=0; o<(size-(i+1)); o++) {
      if(a[o] > a[o+1]) {
        int t = a[o];
        a[o] = a[o+1];
        a[o+1] = t;
      }
    }
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
  
  // --- PHASE 1: REST BASELINE ---
  Serial.println("STEP 1: RELAX your leg completely.");
  Serial.println("Recording starts in 3 seconds...");
  delay(3000);
  Serial.println("--> RECORDING REST BASELINE...");
  
  for(int i = 0; i < CALIBRATION_SAMPLES; i++) {
    qRest[i] = analogRead(EMG_QUADRO_PIN);
    tRest[i] = analogRead(EMG_TWOHEAD_PIN);
    delay(10); // 10ms * 200 = 2000ms (2 seconds)
  }

  // --- PHASE 2: MAX FLEXION ---
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

  // Sort arrays to easily find medians and peaks
  sortArray(qRest, CALIBRATION_SAMPLES);
  sortArray(tRest, CALIBRATION_SAMPLES);
  sortArray(qFlex, CALIBRATION_SAMPLES);
  sortArray(tFlex, CALIBRATION_SAMPLES);

  // Calculate Rest Baseline: Average of the middle elements (ignoring top/bottom 10% noise)
  long qRestSum = 0, tRestSum = 0;
  int restCount = 0;
  for(int i = 20; i < CALIBRATION_SAMPLES - 20; i++) {
    qRestSum += qRest[i];
    tRestSum += tRest[i];
    restCount++;
  }
  int qRestAvg = qRestSum / restCount;
  int tRestAvg = tRestSum / restCount;

  // Calculate Max Peak: Average of the Top 10 highest values (95th percentile)
  long qFlexSum = 0, tFlexSum = 0;
  for(int i = CALIBRATION_SAMPLES - 10; i < CALIBRATION_SAMPLES; i++) {
    qFlexSum += qFlex[i];
    tFlexSum += tFlex[i];
  }
  int qFlexAvg = qFlexSum / 10;
  int tFlexAvg = tFlexSum / 10;

  // --- FINAL THRESHOLD CALCULATION ---
  THRESHOLD_QUADRO = qRestAvg + ((qFlexAvg - qRestAvg) * SENSITIVITY);
  THRESHOLD_TWOHEAD = tRestAvg + ((tFlexAvg - tRestAvg) * SENSITIVITY);

  // Print Results
  Serial.println("\n==================================");
  Serial.println("       CALIBRATION RESULTS        ");
  Serial.println("==================================");
  Serial.printf("QUADRO  -> Rest: %d | Max: %d | SET THRESHOLD: %d\n", qRestAvg, qFlexAvg, THRESHOLD_QUADRO);
  Serial.printf("TWOHEAD -> Rest: %d | Max: %d | SET THRESHOLD: %d\n", tRestAvg, tFlexAvg, THRESHOLD_TWOHEAD);
  Serial.println("==================================");
  Serial.println("Starting main control loop in 3 seconds...\n");
  delay(3000);
}

// --- SETUP FUNCTION ---
void setup() {
  Serial.begin(115200);
  
  legServo.attach(SERVO_PIN);
  legServo.write(currentLegPosition);

  // Run the smart calibration process
  calibrateSensors();
}

// --- SENSOR DATA PROCESSING ---
void processEMG() {
  quadroValue = analogRead(EMG_QUADRO_PIN);
  twoheadValue = analogRead(EMG_TWOHEAD_PIN);

  quadro = (quadroValue > THRESHOLD_QUADRO);
  twohead = (twoheadValue > THRESHOLD_TWOHEAD);
}

// --- SMOOTH POSITION CALCULATION & SERVO CONTROL ---
void calculateSmoothPosition() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= updateInterval) {
    previousMillis = currentMillis;
    bool positionChanged = false;

    if (quadro == true && twohead == false) {
      if (currentLegPosition < 180) {
        currentLegPosition++;
        positionChanged = true;
      }
    } 
    else if (quadro == false && twohead == true) {
      if (currentLegPosition > 0) {
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
  processEMG();
  calculateSmoothPosition();

  unsigned long currentMillis = millis();
  if (currentMillis - previousSerialMillis >= serialInterval) {
    previousSerialMillis = currentMillis;
    
    String legState = "REST (Fixed)";
    if (quadro == true && twohead == false) {
      legState = "EXTENSION ->";
    } else if (quadro == false && twohead == true) {
      legState = "<- FLEXION";
    }

    Serial.printf("Raw Q: %4d | Raw T: %4d | Q_State: %s | T_State: %s | Angle: %3d | State: %s\n", 
                  quadroValue, twoheadValue,
                  quadro ? "TRUE " : "FALSE", 
                  twohead ? "TRUE " : "FALSE", 
                  currentLegPosition, 
                  legState.c_str());
  }

  delay(10);
}