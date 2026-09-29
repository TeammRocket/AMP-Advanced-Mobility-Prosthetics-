# AMP (Advanced Mobility Prosthetics) - Project Context & Guidelines

> **Note for LLMs and AI Assistants:** This README serves as the primary system prompt and context document for this project. Read this thoroughly before making any modifications to the codebase.

## 📌 Project Overview
The AMP project is an ESP32-S3 based IoT Bionic Prosthesis controller. It processes dual-channel EMG signals (Quadriceps and Biceps Femoris / Twohead) to control a robotic servo mechanism. It features an onboard Web UI for telemetry and an ESP-NOW pipeline for low-latency wireless communication.

## 🏗️ Architectural Guidelines (CRITICAL)
- **Procedural / Minimal OOP:** Do not over-engineer. The project intentionally avoids heavy object-oriented wrappers (e.g., no `ServoController` or `WebServerHandler` classes). Keep it C-style procedural with simple structs and global state where appropriate.
- **Concise English Comments:** All code comments must be in English, strictly functional, and extremely short. No "fluff" or redundant explanations.
- **Main Loop Consistency:** `src/main.cpp` acts as the primary orchestrator. Auxiliary logic is split into dedicated files (Calibration, DSP, ESP-NOW, Wi-Fi), but the core state machine lives in `main.cpp`.

## 🧠 DSP Pipeline (Signal Processing)
The EMG signal processing is heavily optimized for the ESP32-S3 FPU and operates at a **1000Hz (1ms) sample rate**. The pipeline consists of two priorities:

1. **Priority 2 (Pre-processing in `src/dsp_filters.cpp`):**
   - **50Hz Notch Filter (Q=10):** Removes powerline interference.
   - **20Hz High-Pass Filter (Q=0.707):** Removes low-frequency motion artifacts.
2. **Priority 1 (Envelope & Lockout in `src/main.cpp` & `src/dsp_filters.cpp`):**
   - **RMS Envelope (`RmsFilter`):** Replaces basic EMA smoothing for a more accurate power envelope.
   - **Deadband / Noise Gate (`DEADBAND_RATIO = 0.15`):** Zeros out micro-fluctuations below 15% of the activation threshold.
   - **Crosstalk Lockout (Antagonist Invalidation):** If both muscles fire simultaneously, the system requires one to dominate by at least 30%. If neither dominates, both signals are zeroed to prevent servo twitching.

## 📁 Codebase Structure
* **`src/main.cpp`**: Core control loop, web server definitions, and Priority 1 DSP thresholding logic.
* **`src/dsp_filters.cpp` / `.h`**: Lightweight Biquad (IIR) and RMS filter implementations. Filters are applied at 1000Hz.
* **`src/calibration.cpp` / `.h`**: Smart dual-muscle calibration routine. Captures rest and flex baselines by passing ADC reads through the DSP filters to ensure accurate thresholds.
* **`src/esp_now_handler.cpp` / `.h`**: ESP-NOW pairing and high-frequency telemetry.
* **`src/wifi_setup.cpp` / `.h`**: Wi-Fi configuration and captive portal logic.
* **`data/`**: LittleFS-served web interface. Separated into `index.html`, `style.css`, and `script.js`.

## 🔧 Hardware & Pinout
* **MCU:** ESP32-S3
* **Quadro EMG Pin:** 4
* **Twohead EMG Pin:** 1
* **Servo PWM Pin:** 2
* **Sample Rate:** Filters and calibration loops target exactly 1000Hz (1ms delay loops).

## 🚀 Getting Started for Development
1. **Build Environment:** The project is built using PlatformIO.
2. **Web Files:** When editing the UI, remember to upload the LittleFS filesystem image via PlatformIO (`Upload Filesystem Image`).
3. **Adding Features:** When adding new features, follow the existing pattern: create a lightweight `.h`/`.cpp` pair without complex classes, and wire it into `main.cpp`.
