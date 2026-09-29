#pragma once
#include <Arduino.h>

// ESP-NOW payload
typedef struct struct_message {
  uint8_t type;
  uint8_t macAddr[6];
  int angle;
} struct_message;

extern bool receiverConnected;
extern String rxMacString;

// Init ESP-NOW broadcast and receive callback
bool initEspNow();

// Send broadcast sync packet (type 0) to search for receiver
void syncEspNowReceiver();

// Send target angle (type 2) to paired receiver
void sendEspNowAngle(int angle);
