/*
 * Low-Cost Cattle Monitoring and Localization System Using ESP32
 * =============================================================
 * FILE: receiver_mobile_node.ino
 * ROLE: Receiver / Mobile Node (attached to cattle)
 *
 * SETUP STEPS:
 *   1. Flash beacon_transmitter.ino to each of the 3 anchor ESP32s
 *      (set BEACON_ID = 1, 2, 3 respectively).
 *   2. Open Serial Monitor for each beacon and note its MAC address.
 *   3. Paste those MACs into the section below marked **STEP 1**.
 *   4. Place the 3 beacons in a triangle layout. Measure their
 *      physical X/Y positions (in meters) and update **STEP 2**.
 *   5. Hold this mobile ESP32 exactly 1 meter from each beacon,
 *      note the average RSSI, and update RSSI_AT_1M (**STEP 3**).
 *   6. Flash this sketch to the mobile ESP32.
 */

#include <esp_now.h>
#include <WiFi.h>
#include <math.h>

// ── **STEP 1** Beacon MAC Addresses ──────────────────────────────
// Replace with the actual MACs printed by each beacon on startup.
uint8_t beacon1MAC[] = {0x30, 0x76, 0xF5, 0xB9, 0x9F, 0x0C}; // BEACON 1
uint8_t beacon2MAC[] = {0x30, 0x76, 0xF5, 0xB9, 0x7A, 0xEC}; // BEACON 2
uint8_t beacon3MAC[] = {0x30, 0x76, 0xF5, 0xBA, 0x23, 0x1C}; // BEACON 3

// ── **STEP 2** Beacon Physical Positions (meters) ────────────────
// Place beacons in a triangle and measure real distances.
// Example layout (adjust to your actual setup):
//   Beacon 1: bottom-left  (0.0, 0.0)
//   Beacon 2: bottom-right (2.0, 0.0)
//   Beacon 3: top          (0.0, 1.5)
float beacon1X = 0.0, beacon1Y = 0.0;
float beacon2X = 2.0, beacon2Y = 0.0;
float beacon3X = 0.0, beacon3Y = 1.5;

// ── **STEP 3** RSSI Calibration ───────────────────────────────────
// Measure average RSSI from each beacon at exactly 1 meter distance.
// In this project the average across 3 beacons was -65 dBm.
#define RSSI_AT_1M    -65.0f   // Update with your measured value
#define PATH_LOSS_EXP   2.7f   // 2.7 suits indoor/semi-open environments
#define RSSI_SAMPLES      7    // Rolling average window size

// ── Beacon State ──────────────────────────────────────────────────
struct BeaconData {
  int  rssiBuffer[RSSI_SAMPLES];
  int  rssiIndex = 0;
  int  rssiCount = 0;
  float distance = -1;
  bool  active   = false;
  unsigned long lastSeen = 0;
};

BeaconData beacons[3];

// ── Helpers ───────────────────────────────────────────────────────
bool macMatch(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

// Log-distance path loss model: d = 10 ^ ((RSSI_AT_1M - rssi) / (10 * n))
float rssiToDistance(float rssi) {
  return pow(10.0f, (RSSI_AT_1M - rssi) / (10.0f * PATH_LOSS_EXP));
}

float getSmoothedRSSI(BeaconData &b) {
  int count = min(b.rssiCount, RSSI_SAMPLES);
  if (count == 0) return -999;
  long sum = 0;
  for (int i = 0; i < count; i++) sum += b.rssiBuffer[i];
  return (float)sum / count;
}

// ── Trilateration ─────────────────────────────────────────────────
// Solves for (outX, outY) given 3 anchor positions and distances.
bool trilaterate(
  float x1, float y1, float d1,
  float x2, float y2, float d2,
  float x3, float y3, float d3,
  float &outX, float &outY
) {
  float A = 2 * (x2 - x1);
  float B = 2 * (y2 - y1);
  float C = d1*d1 - d2*d2 - x1*x1 + x2*x2 - y1*y1 + y2*y2;

  float D = 2 * (x3 - x2);
  float E = 2 * (y3 - y2);
  float F = d2*d2 - d3*d3 - x2*x2 + x3*x3 - y2*y2 + y3*y3;

  float denom = A * E - B * D;
  if (fabs(denom) < 1e-6) {
    Serial.println("[ERROR] Beacons are collinear — reposition them!");
    return false;
  }

  outX = (C * E - F * B) / denom;
  outY = (A * F - D * C) / denom;
  return true;
}

// ── ESP-NOW Receive Callback ──────────────────────────────────────
typedef struct beacon_message {
  uint8_t beacon_id;
  char    label[16];
} beacon_message;

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  beacon_message msg;
  memcpy(&msg, data, sizeof(msg));

  // Identify which beacon sent this packet by MAC address
  int id = -1;
  if      (macMatch(info->src_addr, beacon1MAC)) id = 0;
  else if (macMatch(info->src_addr, beacon2MAC)) id = 1;
  else if (macMatch(info->src_addr, beacon3MAC)) id = 2;
  else return; // Unknown sender — ignore

  int rssi = info->rx_ctrl->rssi;

  // Store RSSI in circular buffer
  beacons[id].rssiBuffer[beacons[id].rssiIndex] = rssi;
  beacons[id].rssiIndex = (beacons[id].rssiIndex + 1) % RSSI_SAMPLES;
  if (beacons[id].rssiCount < RSSI_SAMPLES) beacons[id].rssiCount++;

  // Compute smoothed distance
  float smoothed = getSmoothedRSSI(beacons[id]);
  beacons[id].distance = rssiToDistance(smoothed);
  beacons[id].active   = true;
  beacons[id].lastSeen = millis();

  Serial.printf("[Beacon %d] RSSI: %d dBm | Avg: %.1f dBm | Dist: %.2f m\n",
    id + 1, rssi, smoothed, beacons[id].distance);
}

// ── Setup ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  delay(500);

  Serial.println("\n=== Indoor Positioning Receiver ===");
  Serial.printf("My MAC: %s\n\n", WiFi.macAddress().c_str());

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("Listening for beacons...\n");
}

// ── Loop ──────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // Check all 3 beacons are active and recently heard (within 2 s)
  bool allActive = true;
  for (int i = 0; i < 3; i++) {
    if (!beacons[i].active || (now - beacons[i].lastSeen > 2000)) {
      allActive = false;
    }
  }

  if (allActive) {
    float posX, posY;
    bool ok = trilaterate(
      beacon1X, beacon1Y, beacons[0].distance,
      beacon2X, beacon2Y, beacons[1].distance,
      beacon3X, beacon3Y, beacons[2].distance,
      posX, posY
    );

    if (ok) {
      Serial.println("=====================================");
      Serial.printf("  B1: %.2fm | B2: %.2fm | B3: %.2fm\n",
        beacons[0].distance, beacons[1].distance, beacons[2].distance);
      Serial.printf("Position → X: %.2f m, Y: %.2f m\n", posX, posY);
      Serial.println("=====================================\n");
    }
  } else {
    Serial.println("[Waiting] Need all 3 beacons active...");
  }

  delay(500);
}
