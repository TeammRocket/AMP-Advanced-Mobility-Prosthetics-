#include "dsp_filters.h"
#include <math.h>

BiquadFilter notchQ, hpQ, notchT, hpT;
RmsFilter rmsQ, rmsT;

void BiquadFilter::initHighPass(float fs, float fc, float q) {
    float w0 = 2.0f * PI * fc / fs;
    float alpha = sin(w0) / (2.0f * q);
    float cos_w0 = cos(w0);
    
    float a0 = 1.0f + alpha;
    b0 = ((1.0f + cos_w0) / 2.0f) / a0;
    b1 = -(1.0f + cos_w0) / a0;
    b2 = ((1.0f + cos_w0) / 2.0f) / a0;
    a1 = (-2.0f * cos_w0) / a0;
    a2 = (1.0f - alpha) / a0;
    
    x1 = x2 = y1 = y2 = 0.0f;
}

void BiquadFilter::initNotch(float fs, float fc, float q) {
    float w0 = 2.0f * PI * fc / fs;
    float alpha = sin(w0) / (2.0f * q);
    float cos_w0 = cos(w0);
    
    float a0 = 1.0f + alpha;
    b0 = 1.0f / a0;
    b1 = (-2.0f * cos_w0) / a0;
    b2 = 1.0f / a0;
    a1 = (-2.0f * cos_w0) / a0;
    a2 = (1.0f - alpha) / a0;
    
    x1 = x2 = y1 = y2 = 0.0f;
}

float BiquadFilter::process(float x) {
    float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    return y;
}

void RmsFilter::init(float alpha_val) {
    alpha = alpha_val;
    ema_sq = 0.0f;
}

float RmsFilter::process(float x) {
    ema_sq = (alpha * (x * x)) + ((1.0f - alpha) * ema_sq);
    return sqrt(ema_sq);
}

void initDSP() {
    float fs = 1000.0f; // 1000Hz sampling
    // 50Hz Notch (Priority 2)
    notchQ.initNotch(fs, 50.0f, 10.0f);
    notchT.initNotch(fs, 50.0f, 10.0f);
    
    // 20Hz High-Pass (Priority 2)
    hpQ.initHighPass(fs, 20.0f, 0.707f);
    hpT.initHighPass(fs, 20.0f, 0.707f);
    
    // RMS Envelope (Priority 1)
    rmsQ.init(0.05f);
    rmsT.init(0.05f);
}

float processEMGChannelQ(float rawADC) {
    float v1 = notchQ.process(rawADC);
    float v2 = hpQ.process(v1);
    return rmsQ.process(v2);
}

float processEMGChannelT(float rawADC) {
    float v1 = notchT.process(rawADC);
    float v2 = hpT.process(v1);
    return rmsT.process(v2);
}
