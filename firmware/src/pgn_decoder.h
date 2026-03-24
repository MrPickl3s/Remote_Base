#pragma once

// ============================================================
// Remote Base — J1939 PGN / SPN Decoder
// Passive-listen only. Decodes broadcast PGNs from Cummins ECU.
// Reference: SAE J1939-71 (widely documented in public sources)
// ============================================================

#include <Arduino.h>
#include <stdint.h>

// ============================================================
// Data structures
// ============================================================

// Snapshot of decoded engine parameters
// Fields initialize to sentinel values so main.cpp can tell
// which SPNs have actually been received.
struct EngineData {
  float    rpm             = -1.0f;    // SPN 190  — Engine Speed (RPM)
  float    coolantTempC    = -999.0f;  // SPN 110  — Engine Coolant Temp (°C)
  float    oilPressureKpa  = -1.0f;    // SPN 100  — Engine Oil Pressure (kPa)
  float    vehicleSpeedKmh = -1.0f;    // SPN 84   — Wheel-Based Vehicle Speed (km/h)
  float    batteryVoltage  = -1.0f;    // SPN 168  — Electrical Potential (V)
  uint32_t totalDistKm     = 0;        // SPN 245  — Total Vehicle Distance (km)
  float    ambientTempC    = -999.0f;  // SPN 171  — Ambient Air Temperature (°C)
  bool     valid           = false;    // True once any PGN has been decoded
  unsigned long lastUpdateMs = 0;
};

// One active fault code from DM1
struct DTC {
  uint32_t spn;
  uint8_t  fmi;
  uint8_t  occurrence;    // Occurrence count byte from DM1
};

// Active fault list decoded from DM1 (PGN 65226)
struct FaultList {
  DTC  dtcs[20];          // MAX_FAULTS — matches config.h
  int  count   = 0;
  bool lampOn  = false;   // True if amber or red lamp bit is set in DM1
};

// ============================================================
// PGN constants (decimal values — see J1939 DA tables)
// ============================================================
#define PGN_EEC1   61444UL   // 0xF004  Electronic Engine Controller 1
#define PGN_ET1    65262UL   // 0xFEEE  Engine Temperature 1
#define PGN_EFLP1  65263UL   // 0xFEEF  Engine Fluid Level/Pressure 1
#define PGN_CCVS   65265UL   // 0xFEF1  Cruise Control/Vehicle Speed
#define PGN_VEP    65271UL   // 0xFEF7  Vehicle Electrical Power
#define PGN_VD     65248UL   // 0xFEE0  Vehicle Distance
#define PGN_AMB    65269UL   // 0xFEF5  Ambient Conditions
#define PGN_DM1    65226UL   // 0xFECA  Active Diagnostic Trouble Codes

// J1939 special byte values
#define SPN_NOT_AVAILABLE  0xFF
#define SPN_ERROR          0xFE

// ============================================================
// Individual SPN decoders
// Each takes the raw data bytes (after the 3-byte PGN header),
// writes to the EngineData struct, returns true if value valid.
// ============================================================

// PGN 61444 — EEC1 — Engine Speed
// SPN 190: bytes [3:4] little-endian, 0.125 RPM/bit
inline bool decodeEEC1(const uint8_t* d, int len, EngineData& out) {
  if (len < 5) return false;
  uint16_t raw = (uint16_t)d[4] << 8 | d[3];
  if (raw == 0xFFFF || raw == 0xFE00) return false;
  out.rpm = raw * 0.125f;
  return true;
}

// PGN 65262 — ET1 — Engine Coolant Temperature
// SPN 110: byte [0], 1 °C/bit, offset -40
inline bool decodeET1(const uint8_t* d, int len, EngineData& out) {
  if (len < 1) return false;
  if (d[0] == SPN_NOT_AVAILABLE || d[0] == SPN_ERROR) return false;
  out.coolantTempC = (float)d[0] - 40.0f;
  return true;
}

// PGN 65263 — EFL/P1 — Engine Oil Pressure
// SPN 100: byte [3], 4 kPa/bit
inline bool decodeEFLP1(const uint8_t* d, int len, EngineData& out) {
  if (len < 4) return false;
  if (d[3] == SPN_NOT_AVAILABLE || d[3] == SPN_ERROR) return false;
  out.oilPressureKpa = d[3] * 4.0f;
  return true;
}

// PGN 65265 — CCVS — Vehicle Speed
// SPN 84: bytes [1:2] little-endian, 1/256 km/h per bit
inline bool decodeCCVS(const uint8_t* d, int len, EngineData& out) {
  if (len < 3) return false;
  uint16_t raw = (uint16_t)d[2] << 8 | d[1];
  if (raw == 0xFFFF || raw == 0xFE00) return false;
  out.vehicleSpeedKmh = raw / 256.0f;
  return true;
}

// PGN 65271 — VEP — Battery Voltage
// SPN 168: bytes [4:5] little-endian, 0.05 V/bit
inline bool decodeVEP(const uint8_t* d, int len, EngineData& out) {
  if (len < 6) return false;
  uint16_t raw = (uint16_t)d[5] << 8 | d[4];
  if (raw == 0xFFFF || raw == 0xFE00) return false;
  out.batteryVoltage = raw * 0.05f;
  return true;
}

// PGN 65248 — VD — Vehicle Distance
// SPN 245: bytes [0:3] little-endian, 0.125 km/bit
inline bool decodeVD(const uint8_t* d, int len, EngineData& out) {
  if (len < 4) return false;
  uint32_t raw = (uint32_t)d[3] << 24 | (uint32_t)d[2] << 16 |
                 (uint32_t)d[1] << 8  | d[0];
  if (raw == 0xFFFFFFFF) return false;
  out.totalDistKm = (uint32_t)(raw * 0.125f);
  return true;
}

// PGN 65269 — AMB — Ambient Air Temperature
// SPN 171: bytes [2:3] little-endian, 0.03125 °C/bit, offset -273
inline bool decodeAMB(const uint8_t* d, int len, EngineData& out) {
  if (len < 4) return false;
  uint16_t raw = (uint16_t)d[3] << 8 | d[2];
  if (raw == 0xFFFF || raw == 0xFE00) return false;
  out.ambientTempC = raw * 0.03125f - 273.0f;
  return true;
}

// ============================================================
// DM1 (PGN 65226) — Active Diagnostic Trouble Codes
//
// Byte layout of the already-reassembled payload:
//   Byte 0: Lamp status  (bits 1-0: red stop lamp, bits 3-2: amber warning lamp)
//   Byte 1: Flash status (not used here)
//   Bytes 2+: DTCs, 4 bytes each:
//     [i+0]: SPN bits 7-0
//     [i+1]: SPN bits 15-8
//     [i+2]: SPN bits 18-16 in bits 7-5, FMI in bits 4-0
//     [i+3]: Occurrence count in bits 6-0
//
// NOTE: When more than 2 faults are active, DM1 exceeds 8 bytes
// and is sent via J1939 Transport Protocol (BAM). The FreematicsPlus
// Arduino library does NOT handle BAM reassembly — receiveData() just
// reads one line of hex from the co-processor serial link. Whether the
// STM32 co-processor firmware reassembles BAM internally is unknown
// until tested on real hardware. If it doesn't, DM1 with 3+ faults
// will arrive as multiple partial frames and this decoder will only
// see the first 2 faults. This is acceptable for Phase 0-2.
// ============================================================

inline bool decodeDM1(const uint8_t* d, int len, FaultList& out) {
  out.count  = 0;
  out.lampOn = false;

  if (len < 2) return false;

  // Check lamp status bits: amber (bits 3-2) or red (bits 1-0) lamp on
  // Each lamp field is 2 bits: 00=off, 01=on, 10=error, 11=not available
  uint8_t lampByte = d[0] & 0x0F;
  out.lampOn = (lampByte != 0x00 && lampByte != 0xFF);

  // Parse DTC records starting at byte 2
  for (int i = 2; i + 3 < len && out.count < 20; i += 4) {
    // SPN is 19 bits: byte[i] = bits 7-0, byte[i+1] = bits 15-8,
    // high 3 bits of byte[i+2] = bits 18-16
    uint32_t spn = (uint32_t)d[i]
                 | (uint32_t)d[i+1] << 8
                 | (uint32_t)(d[i+2] & 0xE0) << 11;

    uint8_t fmi        = d[i+2] & 0x1F;    // Low 5 bits of byte 3
    uint8_t occurrence = d[i+3] & 0x7F;    // Low 7 bits of byte 4

    // SPN 0 / FMI 31 is the J1939 "no fault" filler — skip it
    if (spn == 0 && fmi == 31) continue;

    out.dtcs[out.count++] = { spn, fmi, occurrence };
  }

  return out.count > 0;
}

// ============================================================
// dispatchPGN() — main entry point
// buf: raw frame from FreematicsPlus receiveData()
//   buf[0..2] = PGN (24-bit, big-endian)
//   buf[3..]  = data payload
// Updates eng or faults depending on PGN.
// Returns true if the PGN was recognized.
// ============================================================

inline bool dispatchPGN(const uint8_t* buf, int bufLen,
                        EngineData& eng, FaultList& faults) {
  if (bufLen < 3) return false;

  uint32_t pgn      = (uint32_t)buf[0] << 16 | (uint32_t)buf[1] << 8 | buf[2];
  const uint8_t* d  = buf + 3;
  int dLen          = bufLen - 3;
  bool decoded      = false;

  switch (pgn) {
    case PGN_EEC1:  decoded = decodeEEC1(d, dLen, eng);  break;
    case PGN_ET1:   decoded = decodeET1(d, dLen, eng);   break;
    case PGN_EFLP1: decoded = decodeEFLP1(d, dLen, eng); break;
    case PGN_CCVS:  decoded = decodeCCVS(d, dLen, eng);  break;
    case PGN_VEP:   decoded = decodeVEP(d, dLen, eng);   break;
    case PGN_VD:    decoded = decodeVD(d, dLen, eng);    break;
    case PGN_AMB:   decoded = decodeAMB(d, dLen, eng);   break;
    case PGN_DM1:   decoded = decodeDM1(d, dLen, faults); break;
    default:        return false;
  }

  if (decoded && pgn != PGN_DM1) {
    // Only mark engine data valid for actual engine parameter PGNs.
    // DM1 updates the faults struct, not engine data — setting eng.valid
    // on DM1 would suppress the "waiting for PGN broadcasts" message
    // before any real engine data has arrived.
    eng.valid       = true;
    eng.lastUpdateMs = millis();
  }
  return decoded;
}
