#include "calibration.h"
#include "dsp_filters.h"
#include <algorithm>

const int CALIBRATION_SAMPLES = 200;
const float SENSITIVITY = 0.40f;

void calibrateSensors(int pinQuadro, int pinTwohead) {
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

  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    float sumQ = 0, sumT = 0;
    for (int j = 0; j < 10; j++) {
      sumQ += processEMGChannelQ((float)analogRead(pinQuadro) * emgGain);
      sumT += processEMGChannelT((float)analogRead(pinTwohead) * emgGain);
      delay(1);
    }
    qRest[i] = (int)(sumQ / 10.0f);
    tRest[i] = (int)(sumT / 10.0f);
    yield();
  }

  Serial.println("\nSTEP 2: Prepare to FLEX your muscles to the MAXIMUM!");
  Serial.println("Flex in 3..."); delay(1000);
  Serial.println("2..."); delay(1000);
  Serial.println("1..."); delay(1000);
  Serial.println("--> FLEX NOW! HOLD IT! RECORDING MAX PEAK...");

  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    float sumQ = 0, sumT = 0;
    for (int j = 0; j < 10; j++) {
      sumQ += processEMGChannelQ((float)analogRead(pinQuadro) * emgGain);
      sumT += processEMGChannelT((float)analogRead(pinTwohead) * emgGain);
      delay(1);
    }
    qFlex[i] = (int)(sumQ / 10.0f);
    tFlex[i] = (int)(sumT / 10.0f);
    yield();
  }

  Serial.println("\n--> PROCESSING DATA...");

  std::sort(qRest, qRest + CALIBRATION_SAMPLES);
  std::sort(tRest, tRest + CALIBRATION_SAMPLES);
  std::sort(qFlex, qFlex + CALIBRATION_SAMPLES);
  std::sort(tFlex, tFlex + CALIBRATION_SAMPLES);

  long qRestSum = 0, tRestSum = 0;
  int restCount = 0;
  for (int i = 20; i < CALIBRATION_SAMPLES - 20; i++) {
    qRestSum += qRest[i];
    tRestSum += tRest[i];
    restCount++;
  }
  int qRestAvg = (restCount > 0) ? (qRestSum / restCount) : 0;
  int tRestAvg = (restCount > 0) ? (tRestSum / restCount) : 0;

  long qFlexSum = 0, tFlexSum = 0;
  for (int i = CALIBRATION_SAMPLES - 10; i < CALIBRATION_SAMPLES; i++) {
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
