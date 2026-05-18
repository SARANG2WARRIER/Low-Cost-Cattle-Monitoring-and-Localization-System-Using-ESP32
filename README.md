# Low-Cost Cattle Monitoring and Localization System Using ESP32

An indoor/field localization system that uses three ESP32 anchor nodes (beacons) and one mobile ESP32 node attached to cattle. Position is estimated via **RSSI-based trilateration** over **ESP-NOW** — no Wi-Fi router or internet connection required.

---

## How It Works

```
[Beacon 1] ──┐
[Beacon 2] ──┼──(ESP-NOW broadcast)──► [Mobile Node on Cattle]
[Beacon 3] ──┘                              │
                                            ▼
                               Trilateration → (X, Y) position
```

1. Each **anchor/beacon** ESP32 broadcasts its ID at 200 ms intervals via ESP-NOW.
2. The **mobile node** receives these broadcasts and reads the RSSI (signal strength) from each beacon.
3. RSSI is converted to distance using the **log-distance path loss model**.
4. Three distances feed into a **trilateration algorithm** to compute the cattle's (X, Y) position.

---

## Repository Structure

```
├── beacon_transmitter/
│   └── beacon_transmitter.ino   # Flash to each of the 3 anchor ESP32s
├── receiver_mobile_node/
│   └── receiver_mobile_node.ino # Flash to the mobile ESP32 (on the cattle)
└── README.md
```

---

## Hardware Required

| Component | Quantity |
|---|---|
| ESP32 development board | 4 (3 beacons + 1 mobile) |
| USB cables (for flashing) | 4 |
| Power source for beacons | 3 (USB power bank or 5 V adapter) |
| Battery pack for mobile node | 1 |

---

## Setup & Flashing Guide

### Step 1 — Get MAC Addresses of Each Beacon ESP32

1. Open `beacon_transmitter.ino` in Arduino IDE.
2. Set `#define BEACON_ID 1`.
3. Flash to the first ESP32.
4. Open **Serial Monitor** at **115200 baud**.
5. Note the MAC address printed (e.g. `30:76:F5:B9:9F:0C`).
6. Repeat for Beacon 2 (`BEACON_ID 2`) and Beacon 3 (`BEACON_ID 3`).

Recorded MAC addresses from this project:

| Node | MAC Address |
|---|---|
| Beacon 1 | `30:76:F5:B9:9F:0C` |
| Beacon 2 | `30:76:F5:BA:23:1C` |
| Beacon 3 | `30:76:F5:B9:7A:EC` |
| Mobile Node | `30:76:F5:BA:16:28` |

### Step 2 — Calibrate RSSI at 1 Meter

1. Place the mobile ESP32 exactly **1 metre** from each beacon (one at a time).
2. Read the average RSSI printed on the Serial Monitor.
3. Average the values across all 3 beacons.
4. Update `#define RSSI_AT_1M` in `receiver_mobile_node.ino`.

Calibration results from this project:

| Beacon | RSSI at 1 m |
|---|---|
| Beacon 1 | −66 dBm |
| Beacon 2 | −64 dBm |
| Beacon 3 | −66 dBm |
| **Average used** | **−65 dBm** |

### Step 3 — Set Physical Beacon Positions

Arrange the 3 beacons in a triangle. Measure their real-world positions in metres and update the coordinate constants in `receiver_mobile_node.ino`:

```cpp
float beacon1X = 0.0, beacon1Y = 0.0;  // bottom-left
float beacon2X = 2.0, beacon2Y = 0.0;  // bottom-right
float beacon3X = 0.0, beacon3Y = 1.5;  // top
```

### Step 4 — Flash the Mobile Node

1. Open `receiver_mobile_node.ino`.
2. Paste the beacon MACs from Step 1.
3. Update positions from Step 3 and RSSI value from Step 2.
4. Flash to the 4th ESP32 (the mobile node).
5. Open Serial Monitor — you should see live distance and position output.

---

## Serial Monitor Output (Example)

```
=== Indoor Positioning Receiver ===
My MAC: 30:76:F5:BA:16:28

Listening for beacons...

[Beacon 1] RSSI: -67 dBm | Avg: -66.3 dBm | Dist: 1.12 m
[Beacon 2] RSSI: -72 dBm | Avg: -71.8 dBm | Dist: 1.87 m
[Beacon 3] RSSI: -69 dBm | Avg: -68.5 dBm | Dist: 1.43 m
=====================================
  B1: 1.12m | B2: 1.87m | B3: 1.43m
Position → X: 0.97 m, Y: 0.82 m
=====================================
```

---

## Key Parameters

| Parameter | Value | Notes |
|---|---|---|
| `RSSI_AT_1M` | −65 dBm | Measure at your site |
| `PATH_LOSS_EXP` | 2.7 | Suitable for indoor/semi-open |
| `RSSI_SAMPLES` | 7 | Rolling average window |
| Beacon broadcast interval | 200 ms | Adjustable in `loop()` |
| Position update interval | 500 ms | Adjustable in `loop()` |
| Beacon timeout | 2000 ms | If not heard, marked inactive |

---

## Arduino IDE Requirements

- Board: **ESP32 by Espressif** (v3.x recommended)
- Libraries: `esp_now.h`, `WiFi.h`, `math.h` — all built into the ESP32 core, no extra installs needed.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `ESP-NOW init failed` | Make sure `WiFi.mode(WIFI_STA)` is called before `esp_now_init()` |
| `Beacons collinear` error | Reposition beacons so they form a proper triangle, not a straight line |
| Position jumps wildly | Increase `RSSI_SAMPLES` or decrease `PATH_LOSS_EXP` slightly |
| Only 2 beacons detected | Check MAC addresses match exactly; verify all 3 beacons are powered |
| Distances seem off | Re-calibrate `RSSI_AT_1M` at your actual deployment site |

---

## License

MIT License — free to use, modify, and distribute.
