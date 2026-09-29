#include "esp_now_handler.h"
#include <esp_now.h>
#include <WiFi.h>

bool receiverConnected = false;
String rxMacString = "-";

static uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t receiverAddress[6] = {0, 0, 0, 0, 0, 0};
static struct_message myData;
static esp_now_peer_info_t peerInfo;

static void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(struct_message)) {
    struct_message *msg = (struct_message *)incomingData;
    if (msg->type == 1) {
      memcpy(receiverAddress, msg->macAddr, 6);
      receiverConnected = true;
      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               receiverAddress[0], receiverAddress[1], receiverAddress[2],
               receiverAddress[3], receiverAddress[4], receiverAddress[5]);
      rxMacString = String(macStr);

      if (!esp_now_is_peer_exist(receiverAddress)) {
        memset(&peerInfo, 0, sizeof(peerInfo));
        memcpy(peerInfo.peer_addr, receiverAddress, 6);
        peerInfo.channel = WiFi.channel();
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
      }
    }
  }
}

bool initEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Init failed");
    return false;
  }

  esp_now_register_recv_cb(OnDataRecv);
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  Serial.println("[ESP-NOW] Ready (Broadcast mode)");
  return true;
}

void syncEspNowReceiver() {
  myData.type = 0;
  WiFi.macAddress(myData.macAddr);
  esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));
}

void sendEspNowAngle(int angle) {
  if (receiverConnected) {
    myData.type = 2;
    myData.angle = angle;
    esp_now_send(receiverAddress, (uint8_t *)&myData, sizeof(myData));
  }
}
