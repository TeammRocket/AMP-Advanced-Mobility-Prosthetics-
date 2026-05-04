#include <Arduino.h>
#include <ESP32Servo.h>

// --- PIN CONFIGURATION ---
// Replace with your actual ESP32-S3 pins
const int EMG_QUADRO_PIN = 4;
const int EMG_TWOHEAD_PIN = 5;
const int SERVO_PIN = 1;

// --- THRESHOLD VALUES ---
// The signal must exceed this value for the muscle to be considered active
const int THRESHOLD_QUADRO = 1500;
const int THRESHOLD_TWOHEAD = 1500;

// --- GLOBAL VARIABLES FOR WEB INTERFACE ---
int quadroValue = 0;
int twoheadValue = 0;
bool quadro = false;
bool twohead = false;
int currentLegPosition = 90; // Initial angle (0: fully bent, 180: fully extended)

// --- SERVO OBJECT ---
Servo legServo;

// --- NON-BLOCKING TIMER VARIABLES ---
unsigned long previousMillis = 0;
const long updateInterval = 15; // Speed of movement in milliseconds (lower is faster)

// Variables for controlling Serial Monitor output speed
unsigned long previousSerialMillis = 0;
const long serialInterval = 500; // Output data every 500 milliseconds

// --- SETUP FUNCTION (Runs once at startup) ---
void setup() {
  Serial.begin(115200);

  // Initialize the servo motor
  legServo.attach(SERVO_PIN);
  legServo.write(currentLegPosition); // Set motor to initial position (90 degrees)
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

// --- SMOOTH POSITION CALCULATION & SERVO CONTROL ---
void calculateSmoothPosition() {
  unsigned long currentMillis = millis();

  // Update position only if the specified interval has passed
  if (currentMillis - previousMillis >= updateInterval) {
    previousMillis = currentMillis;

    // Flag to check if the motor needs to move
    bool positionChanged = false;

    // --- MAIN CONTROL LOGIC ---
    if (quadro == true && twohead == false) {
      // Extension: smoothly increase the angle
      if (currentLegPosition < 180) {
        currentLegPosition++;
        positionChanged = true;
      }
    } 
    else if (quadro == false && twohead == true) {
      // Flexion: smoothly decrease the angle
      if (currentLegPosition > 0) {
        currentLegPosition--;
        positionChanged = true;
      }
    }

    // If the angle has changed, physically move the servo motor
    if (positionChanged) {
      legServo.write(currentLegPosition);
    }
  }
}

// --- MAIN LOOP (Runs continuously) ---
void loop() {
  // 1. Update sensor data
  processEMG();

  // 2. Calculate new smooth position
  calculateSmoothPosition();

  // 3. Debug output to Serial Monitor
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

  // 4. Critical tiny delay to allow ESP32 background tasks to process
  delay(10);
}