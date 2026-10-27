# ESP32 Offline Bell, Schedule & 128x64 HUB75 Matrix Wiring Guide

This document provides complete, pin-by-pin hardware connection instructions for the entire system.

---

## 1. Power Supply Architecture

> [!IMPORTANT]
> A 128x64 RGB LED Matrix panel consumes between **2.5A to 4.0A at 5V** when displaying bright content.
> **DO NOT** attempt to power the LED matrix directly from the ESP32's 3.3V or 5V (VIN) pin via USB. Use a dedicated **5V 4A+ (or 5A) DC power supply**.

```
 +-------------------------------------------------------+
 |                  5V 5A DC POWER SUPPLY                |
 +---------------------------+---------------------------+
                             |
             +---------------+---------------+
             |                               |
             v (5V / GND)                    v (5V / GND)
   +-------------------+           +-----------------------+
   |   HUB75 MATRIX    |           |  ESP32 (VIN / GND)    |
   | (128x64 Display)  |           +-----------+-----------+
   +-------------------+                       |
                               +---------------+---------------+
                               |                               |
                               v (3.3V / GND)                  v (5V / GND)
                     +-------------------+           +-------------------+
                     |  GPS NEO-6M / 8M  |           |   RELAY MODULE    |
                     +-------------------+           +-------------------+
```

- Connect the **GND** (Ground) of the Power Supply, ESP32, HUB75 Matrix, GPS Module, and Relay Module together (**Common Ground is mandatory**).

---

## 2. Pin Connection Tables

### A. HUB75 LED Matrix (128x64 DMA, 1 Chain) to ESP32

The display is configured for a single 128x64 HUB75 matrix panel (`PANEL_RES_X 128`, `PANEL_RES_Y 64`, `PANEL_CHAIN 1`, 1/32 scan with `E` line). Connect the ESP32 GPIOs directly to the `DATA IN` connector of the panel.

| HUB75 Pin | Pin Name | ESP32 GPIO | Description |
|:---:|:---:|:---:|:---|
| 1 | **R1** | **GPIO 25** | Red pixel data (Top half) |
| 2 | **G1** | **GPIO 26** | Green pixel data (Top half) |
| 3 | **B1** | **GPIO 27** | Blue pixel data (Top half) |
| 4 | **GND** | **GND** | Ground |
| 5 | **R2** | **GPIO 14** | Red pixel data (Bottom half) |
| 6 | **G2** | **GPIO 12** | Green pixel data (Bottom half) |
| 7 | **B2** | **GPIO 13** | Blue pixel data (Bottom half) |
| 8 | **E** | **GPIO 23** | Row Address E (required for 1/32 scan 64-row panels) |
| 9 | **A** | **GPIO 19** | Row Address A |
| 10 | **B** | **GPIO 5** | Row Address B |
| 11 | **C** | **GPIO 33** | Row Address C |
| 12 | **D** | **GPIO 18** | Row Address D |
| 13 | **CLK** | **GPIO 4** | Shift Clock |
| 14 | **LAT** | **GPIO 15** | Data Latch |
| 15 | **OE** | **GPIO 32** | Output Enable (Active Low) |
| 16 | **GND** | **GND** | Ground |

---

### B. GPS Module (NEO-6M / NEO-8M) to ESP32

| GPS Module Pin | ESP32 Pin | Notes |
|:---:|:---:|:---|
| **VCC** | **3.3V** or **5V** | Check your GPS board voltage regulator (most accept 3.3V–5V) |
| **GND** | **GND** | Common Ground |
| **TX** | **GPIO 16 (RX2)** | Serial data from GPS to ESP32 |
| **RX** | **GPIO 17 (TX2)** | Configuration commands from ESP32 to GPS (optional) |

> [!TIP]
> Place the GPS ceramic patch antenna near a window or outdoors facing the sky for fast satellite lock (usually < 45 seconds).

---

### C. Relay Actuator & AC Bell to ESP32

| Relay Module Pin | ESP32 / Power Pin | Notes |
|:---:|:---:|:---|
| **VCC** | **5V (VIN)** | Powers the relay coil |
| **GND** | **GND** | Common Ground |
| **IN** | **GPIO 22** | Logic trigger signal |

#### AC Electric Bell Connection (High Voltage):
- Connect AC Live wire to Relay **COM** (Common).
- Connect Relay **NO** (Normally Open) to the AC Bell Live terminal.
- Connect AC Neutral wire directly to the AC Bell Neutral terminal.
- *(Recommended)* Install an RC snubber (100Ω + 0.1µF 400V) or MOV across the relay contacts to suppress high-voltage inductive arcing when the bell turns off.

---

### D. Manual Bell Push Button

| Button Terminal | ESP32 Pin | Notes |
|:---:|:---:|:---|
| Terminal 1 | **GPIO 21** | Configured with internal software pull-up |
| Terminal 2 | **GND** | Pressing connects GPIO 21 to GND |
