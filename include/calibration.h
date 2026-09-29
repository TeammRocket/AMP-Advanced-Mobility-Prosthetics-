#pragma once
#include <Arduino.h>

extern int THRESHOLD_QUADRO;
extern int THRESHOLD_TWOHEAD;
extern float smoothedQuadro;
extern float smoothedTwohead;
extern float emgGain;

// Run smart dual-muscle calibration
void calibrateSensors(int pinQuadro, int pinTwohead);
