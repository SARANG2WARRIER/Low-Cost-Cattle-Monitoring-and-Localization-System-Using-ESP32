/*
 * Low-Cost Cattle Monitoring and Localization System Using ESP32
 * =============================================================
 * FILE: beacon_transmitter.ino
 * ROLE: Transmitter / Anchor Node (Beacon)
 *
 * USAGE:
 *   - Flash this to each of the 3 anchor ESP32 boards.
 *   - Change BEACON_ID to 1, 2, or 3 before flashing each one.
 *   - Note the MAC address printed on Serial Monitor and update
 *     receiver_mobile_node.ino with those addresses.
 */

#include <esp_now.h>
#include <WiFi.h>

// ── Change this to 1, 2, or 3 for each beacon board ─────────────
#define BEACON_ID 1

// ── Broadcast to all devices ─────────────────────────────────────
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ── Message Structure ─────────────────────────────────────────────
typedef struct beacon_message {
  uint8_t beacon_id;
  char label[16];
} beacon_message;

beacon_message myData;
esp_now_peer_info_t peerInfo;

// ── Send Callback (ESP32 core v3.x signature) ─────────────────────
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // Optional: Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sent OK" : "Send Failed");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  delay(500);

  Serial.printf("Beacon %d running. MAC: %s\n", BEACON_ID, WiFi.macAddress().c_str());

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  myData.beacon_id = BEACON_ID;
  snprintf(myData.label, sizeof(myData.label), "BEACON_%d", BEACON_ID);
}

void loop() {
  esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));
  delay(200); // Broadcast every 200ms
}
