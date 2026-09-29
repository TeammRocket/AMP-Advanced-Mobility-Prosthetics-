#pragma once

#include <Arduino.h>

// Simple procedural Biquad Filter (IIR)
struct BiquadFilter {
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;
    
    void initHighPass(float fs, float fc, float q);
    void initNotch(float fs, float fc, float q);
    float process(float x);
};

// RMS Envelope Filter
struct RmsFilter {
    float alpha;
    float ema_sq;
    void init(float alpha_val);
    float process(float x);
};

void initDSP();
float processEMGChannelQ(float rawADC);
float processEMGChannelT(float rawADC);
