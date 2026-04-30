#include <Arduino.h>

// --- PIN CONFIGURATION ---
const int EMG_QUADRO_PIN = 4;
const int EMG_TWOHEAD_PIN = 5;

// --- THRESHOLD VALUES ---
// The signal must exceed this value for the muscle to be considered active
const int THRESHOLD_QUADRO = 1500;
const int THRESHOLD_TWOHEAD = 1500;

// --- GLOBAL VARIABLES FOR WEB INTERFACE ---
int quadroValue = 0;
int twoheadValue = 0;
bool quadro = false;
bool twohead = false;
int currentLegPosition = 90; // Virtual position (0: fully bent, 180: fully extended)

// --- NON-BLOCKING TIMER VARIABLES ---
unsigned long previousMillis = 0;
const long updateInterval = 15; // Speed of movement in milliseconds (lower is faster)

// Змінні для контролю виводу в Serial Monitor
unsigned long previousSerialMillis = 0;
const long serialInterval = 500; // Виводити дані кожні 500 мілісекунд (2 рази на секунду)

// --- SETUP FUNCTION (Runs once at startup) ---
void setup() {
  // Initialize serial communication at 115200 baud rate
  Serial.begin(115200);
}

// --- SENSOR DATA PROCESSING ---
void processEMG() {
  // Read analog values from sensors
  quadroValue = analogRead(EMG_QUADRO_PIN);
  twoheadValue = analogRead(EMG_TWOHEAD_PIN);

  // Determine muscle state based on thresholds
  quadro = (quadroValue > THRESHOLD_QUADRO);
  twohead = (twoheadValue > THRESHOLD_TWOHEAD);
}

// --- SMOOTH POSITION CALCULATION ---
void calculateSmoothPosition() {
  unsigned long currentMillis = millis();

  // Update position only if the specified interval has passed
  if (currentMillis - previousMillis >= updateInterval) {
    previousMillis = currentMillis;

    // --- MAIN CONTROL LOGIC ---
    if (quadro == true && twohead == false) {
      // Extension: smoothly increase the angle
      if (currentLegPosition < 180) {
        currentLegPosition++;
      }
    } 
    else if (quadro == false && twohead == true) {
      // Flexion: smoothly decrease the angle
      if (currentLegPosition > 0) {
        currentLegPosition--;
      }
    }
    // If both are true or both are false, the position remains unchanged.
  }
}

// --- MAIN LOOP (Runs continuously) ---
void loop() {
  // 1. Update sensor data
  processEMG();

  // 2. Calculate new smooth position
  calculateSmoothPosition();

  // 3. Debug output to Serial Monitor (сповільнений вивід)
  unsigned long currentMillis = millis();
  if (currentMillis - previousSerialMillis >= serialInterval) {
    previousSerialMillis = currentMillis;
    
    // Determine the text state of the leg
    String legState = "REST (Fixed)";
    if (quadro == true && twohead == false) {
      legState = "EXTENSION ->";
    } else if (quadro == false && twohead == true) {
     legState = "<- FLEXION";
    }

    // Print clear and readable text to the console
    Serial.printf("Quadro: %s | Twohead: %s | Angle: %d | State: %s\n", 
                  quadro ? "TRUE" : "FALSE", 
                  twohead ? "TRUE" : "FALSE", 
                  currentLegPosition, 
                  legState.c_str());
  }

  // 4. Critical tiny delay to allow ESP32 background tasks (like USB Serial) to process
  delay(10);
}