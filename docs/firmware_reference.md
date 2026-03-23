# RESOURCE: Remote Base Firmware Development Reference

## Purpose
This document provides Claude Code with everything it needs to help develop firmware for the Remote Base fleet telematics project. It consolidates hardware specs, existing code resources, protocol references, and a recommended build path.

---

## 1. Hardware Platform

**Device:** Freematics ONE+ Model H
- **MCU:** Espressif ESP32 with 16MB Flash, 8MB PSRAM, 32K RTC
- **Co-processor:** STM32 protocol co-processor (handles OBD/CAN communication)
- **Connectivity:** 802.11 b/g/n WiFi, dual-mode Bluetooth (Classic + BLE)
- **Cellular:** SIM7600A-H or SIM7600E-H 4G LTE CAT4 module
- **GNSS:** u-blox M10 module with active ceramic antenna
- **Motion:** ICM-42627 IMU (accelerometer/gyroscope)
- **Storage:** microSD slot (up to 32GB) via SPI
- **OBD Connector:** OBD-II Type B (HD-OBD) male connector — designed for heavy-duty vehicles (24V)
- **CAN Bus:** Built-in CAN transceiver, J1939 protocol supported natively
- **GPIO:** 4-pin Molex socket (GPIO26, GPIO34, VCC, GND)
- **Power:** ~20mA idle, ~50mA WiFi, ~200mA LTE+GNSS active. Low-power mode ~10mA.

**Key Detail:** The Freematics ONE+ Model H communicates with the CAN bus through its STM32 co-processor, NOT directly through the ESP32's native TWAI/CAN peripheral. All CAN/J1939 communication goes through the FreematicsPlus Arduino library which abstracts this co-processor interface.

**Cellular SIM:** Hologram IoT SIM (already selected for the project)

---

## 2. Existing Code Resources

### 2a. Freematics Official Repository
**Repo:** https://github.com/stanleyhuangyc/Freematics
**Branch:** master
**Key directories:**
- `firmware_v5/` — All ESP32-based ONE+ sketches
- `libraries/` — Arduino libraries for ONE+ hardware

### 2b. J1939 Monitor Sketch (YOUR STARTING POINT)
**File:** `firmware_v5/j1939_monitor/j1939_monitor.ino`
**What it does:** Reads J1939 broadcast PGN data from the CAN bus and prints it to serial.
**Written for:** Freematics ONE+ Model H specifically.

```cpp
// Core pattern from the J1939 monitor:
#include <FreematicsPlus.h>

FreematicsESP32 sys;
COBD obd;

void setup() {
  Serial.begin(115200);
  while (!sys.begin());           // Initialize co-processor
  obd.begin(sys.link);            // Initialize OBD library
  while (!obd.init(PROTO_J1939)); // Start J1939 protocol
}

void loop() {
  byte buf[128];
  int bytes = obd.receiveData(buf, sizeof(buf));
  if (bytes > 0 && bytes >= 3) {
    // First 3 bytes = PGN (24-bit)
    uint32_t pgn = (uint32_t)buf[0] << 16 | (uint32_t)buf[1] << 8 | buf[2];
    // Remaining bytes = data payload (up to 8 bytes for standard frames)
    // Parse SPNs from data based on PGN definition
  }
}
```

### 2c. Telelogger Sketch (CELLULAR TRANSMISSION REFERENCE)
**File:** `firmware_v5/telelogger/`
**What it does:** Full telematics data logger that collects OBD, GPS, and IMU data, stores to SD, and transmits over WiFi or 4G cellular to Freematics Hub or Traccar.
**Key capabilities to reference:**
- Circular buffer in PSRAM for data during network outages
- UDP and HTTP(S) transmission protocols
- Traccar GPS tracking platform integration (OsmAnd protocol)
- BLE remote control/monitoring via Freematics Controller App
- Low-power mode management
- Hologram-compatible cellular initialization

### 2d. FreematicsPlus Library
**Location:** `libraries/FreematicsPlus/`
**Key classes:**
- `FreematicsESP32` — System initialization, power management
- `COBD` — OBD/CAN communication (including J1939 via `PROTO_J1939`)
- `CFreematicsSIM7600` (or similar) — Cellular modem control
- GPS parsing handled by co-processor, coordinates available via library calls

---

## 3. J1939 Protocol Quick Reference

### How J1939 Works on This Hardware
1. The Freematics co-processor handles the low-level CAN bus communication
2. When initialized with `PROTO_J1939`, it listens for broadcast CAN frames at 250kbps (J1939 standard baud rate)
3. `obd.receiveData()` returns raw frames: 3 bytes PGN + up to 8 bytes data
4. YOUR firmware must decode the PGN and extract SPNs from the data bytes

### Essential PGNs for Cummins Heavy-Duty Engines

| PGN | Name | Key SPNs | Broadcast Rate |
|------|------|----------|----------------|
| 61444 (0xF004) | Electronic Engine Controller 1 (EEC1) | SPN 190: Engine Speed (RPM) | 10-20ms |
| 65262 (0xFEEE) | Engine Temperature 1 (ET1) | SPN 110: Engine Coolant Temp | 1s |
| 65263 (0xFEEF) | Engine Fluid Level/Pressure 1 (EFL/P1) | SPN 100: Engine Oil Pressure | 500ms |
| 65270 (0xFEF6) | Inlet/Exhaust Conditions 1 (IC1) | SPN 102: Boost Pressure | 500ms |
| 65266 (0xFEF2) | Fuel Economy (LFE) | SPN 183: Fuel Rate | 100ms |
| 65248 (0xFEE0) | Vehicle Distance | SPN 245: Total Vehicle Distance | 5s |
| 65253 (0xFEE5) | Engine Hours, Revolutions (HOURS) | SPN 247: Total Engine Hours | On request |
| 65269 (0xFEF5) | Ambient Conditions (AMB) | SPN 171: Ambient Air Temp | 1s |
| 65265 (0xFEF1) | Cruise/Vehicle Speed (CCVS) | SPN 84: Vehicle Speed | 100ms |
| 65226 (0xFECA) | Active DTC (DM1) | Active fault codes | 1s |
| 65227 (0xFECB) | Previously Active DTC (DM2) | Stored fault codes | On request |
| 65259 (0xFEEB) | Component ID | SPN 586: Make/Model | On request |
| 65260 (0xFEEC) | Vehicle ID (VIN) | SPN 237: VIN | On request |

### SPN Decoding Pattern
Each SPN has a defined position in the PGN's 8-byte data field, plus a resolution (scale) and offset:

```
Physical Value = (Raw Value × Resolution) + Offset
```

**Example — Engine Speed (SPN 190 in PGN 61444):**
- Byte position: bytes 4-5 (0-indexed: data[3] and data[4])
- Resolution: 0.125 RPM/bit
- Offset: 0
- Raw value: `(uint16_t)data[4] << 8 | data[3]` (little-endian)
- Physical: `raw * 0.125` = RPM

**Example — Engine Coolant Temp (SPN 110 in PGN 65262):**
- Byte position: byte 1 (0-indexed: data[0])
- Resolution: 1 °C/bit
- Offset: -40 °C
- Physical: `data[0] - 40` = °C

**Special values:** `0xFF` (255) = Not Available, `0xFE` (254) = Error

### Important Notes for Cummins Engines
- Standard J1939 PGNs work across all Cummins engines (ISX, ISB, ISL, etc.)
- Cummins may use proprietary PGNs in range 0x00FF00–0x00FFFF for engine-specific data
- Engine Hours and Total Fuel Used are NOT periodically broadcast — they must be requested via the Request PGN (59904 / 0xEA00)
- DTC (fault code) messages use DM1 for active faults and DM2 for stored faults
- The J1939 bus on heavy-duty trucks typically runs at 250 kbps

---

## 4. Recommended Build Path

### Phase 1: Read and Display J1939 Data (Week 1)
**Goal:** Get the device reading real PGN data from a Cummins engine and displaying it on serial.

1. Clone the Freematics repo: `git clone https://github.com/stanleyhuangyc/Freematics.git`
2. Start from `firmware_v5/j1939_monitor/j1939_monitor.ino`
3. Add PGN decoding for the essential parameters listed above
4. Create a struct to hold decoded engine data:

```cpp
struct EngineData {
  float rpm;              // SPN 190
  float coolantTemp;      // SPN 110 (°C)
  float oilPressure;      // SPN 100 (kPa)
  float boostPressure;    // SPN 102 (kPa)
  float fuelRate;         // SPN 183 (L/hr)
  float vehicleSpeed;     // SPN 84 (km/h)
  uint32_t totalDistance;  // SPN 245 (km)
  uint32_t engineHours;   // SPN 247 (hours)
  float ambientTemp;      // SPN 171 (°C)
  bool hasActiveDTC;      // DM1 present
  unsigned long lastUpdate; // millis() timestamp
};
```

5. Test on a real truck. Verify PGN data matches what you see on the truck's dashboard.

### Phase 2: Add GPS and Cellular (Week 2)
**Goal:** Transmit engine data + GPS position over cellular.

1. Reference the `telelogger` sketch for:
   - Cellular modem initialization (SIM7600 with Hologram SIM)
   - GPS initialization and coordinate retrieval
   - Network connection and data transmission
2. Define your telemetry data format (JSON or compact binary)
3. Set up a receiving endpoint (Traccar is free and open-source, or build a simple HTTP endpoint)
4. Implement buffering (store to SD or PSRAM when cellular is unavailable)

### Phase 3: Add SD Logging and Fault Handling (Week 3)
**Goal:** Local data backup and DTC monitoring.

1. Log all telemetry to microSD as CSV or structured format
2. Parse DM1 messages for active fault codes
3. Decode SPN + FMI from DTC data to identify specific faults
4. Implement alert logic for critical conditions (overtemp, low oil pressure, etc.)

### Phase 4: Server-Side (Week 4+)
**Goal:** Dashboard and fleet visibility.

1. Stand up Traccar or custom server to receive telemetry
2. Build or configure a dashboard for fleet operators
3. Implement historical data storage and querying

---

## 5. Additional Resources

### Open-Source J1939 Libraries (if deeper protocol work needed)
- **Open-SAE-J1939** by DanielMartensson: https://github.com/DanielMartensson/Open-SAE-J1939
  - 580+ stars, ANSI C, works on any platform including ESP32
  - Full J1939 stack: address claiming, transport protocol (BAM/RTS-CTS), diagnostics
  - Written by an M.Sci engineer/teacher at Institution of Applied Hydraulics (Sweden)
  - Note: "as-is" for educational/demo use; no technical support
  - Useful if you need features beyond simple PGN reading (e.g., sending requests, multi-packet messages)
  
- **ARD1939** by Copperhill Technologies: https://copperhilltech.com/blog/sae-j1939-protocol-stack-sketch-for-esp32-using-the-arduino-ide/
  - Free ESP32-compatible J1939 stack for Arduino IDE
  - Includes network scanner and USB gateway examples
  - Same company (Copperhill) that Freematics used to test their Model H with J1939 ECU Simulator
  - Companion book: "SAE J1939 ECU Programming & Vehicle Bus Simulation with Arduino"

### Embedder.com (AI Firmware Assistant)
- Free for individual developers at https://embedder.com
- Can generate ESP32 peripheral driver code from datasheets
- Useful for: CAN bus initialization, SPI/SD card drivers, cellular modem AT commands
- Limitation: Handles low-level peripheral code, not J1939 protocol layer

### Freematics Documentation
- Product page: https://freematics.com/pages/products/freematics-one-plus-model-h/
- Blog (with J1939 testing notes): https://blog.freematics.com
- Freematics Builder tool for non-programmers to configure/upload firmware

### J1939 Protocol References
- Copperhill Technologies guide: https://copperhilltech.com/blog/guide-to-sae-j1939-parameter-group-numbers-pgn/
- CSS Electronics explainer: https://www.csselectronics.com/pages/j1939-explained-simple-intro-tutorial
- SAE J1939 Companion Spreadsheet (purchasable from SAE) — definitive PGN/SPN reference

---

## 6. Key Decisions & Constraints

- **Arduino IDE vs PlatformIO:** Freematics examples use Arduino IDE. PlatformIO is recommended for serious development (better dependency management, debugging).
- **Data format:** JSON is easier to debug but larger. Consider compact binary for cellular bandwidth savings.
- **Cellular costs:** Hologram charges per byte. Optimize transmission frequency and payload size.
- **Baud rate:** J1939 standard is 250 kbps. The Freematics co-processor handles this, but verify with `PROTO_J1939`.
- **No SAE paywall needed:** The essential PGN/SPN definitions for common engine parameters are widely published. Cummins-specific proprietary PGNs would require SAE documents, but standard parameters cover 90%+ of fleet monitoring needs.
