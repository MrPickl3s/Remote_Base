// ============================================================
// Remote Base — Main Firmware
// Freematics ONE+ Model H | ESP32 | Arduino framework
//
// Phase 0: Compiling skeleton — reads J1939, decodes PGNs,
//          prints engine data to serial, detects DM1 fault codes.
// Phase 1 (TODO): Cellular transmission via SIM7600 + HTTP POST
// Phase 2 (TODO): Store-and-forward to microSD when offline
// ============================================================

#include <Arduino.h>
#include <FreematicsPlus.h>
#include "config.h"
#include "pgn_decoder.h"

// ============================================================
// Globals
// ============================================================

FreematicsESP32 sys;
COBD obd;

EngineData eng;
FaultList  faults;

// Frame counters shown in the status line
uint32_t framesTotal   = 0;
uint32_t framesKnown   = 0;
uint32_t framesUnknown = 0;

// Unique PGNs seen — small fixed array for diagnostics
static uint32_t seenPgns[64];
static int      seenPgnCount = 0;

// Fault debounce: only alert after a fault persists for DM1_DEBOUNCE_MS
static bool          faultPending    = false;
static unsigned long faultFirstSeenMs = 0;

// Print engine data every 5 seconds
#define PRINT_INTERVAL_MS 5000
static unsigned long lastPrintMs = 0;

// ============================================================
// Forward declarations
// ============================================================
void printEngineData();
void checkFaultAlert();
bool isKnownPgn(uint32_t pgn);
void trackUniquePgn(uint32_t pgn);

// ============================================================
// setup()
// ============================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  Serial.println("\n========================================");
  Serial.println("Remote Base v" FIRMWARE_VER);
  Serial.print("Bus ID: ");
  Serial.println(BUS_ID);
  Serial.println("========================================");
  Serial.println("Initializing Freematics co-processor...");

  // Boot the STM32 co-processor that handles CAN/OBD communication.
  // The co-processor bridges the ESP32 to the physical CAN bus.
  if (!sys.begin()) {
    Serial.println("FATAL: Co-processor init failed. Check USB power and hardware.");
    // Halt — there is no point continuing without the co-processor
    while (true) delay(1000);
  }

  // Connect the OBD library to the co-processor link
  obd.begin(sys.link);

  // Initialize J1939 passive-listen mode (250 kbps broadcast)
  // This may take a few attempts if the bus is not yet active.
  Serial.println("Waiting for J1939 bus...");
  int attempts = 0;
  bool busOk = false;
  while (attempts < 5) {
    if (obd.init(PROTO_J1939)) {
      busOk = true;
      break;
    }
    Serial.print("  Attempt ");
    Serial.print(++attempts);
    Serial.println(" failed — retrying in 2s");
    delay(2000);
  }

  if (busOk) {
    Serial.println("J1939 bus active — listening for PGN broadcasts");
  } else {
    // Hardware may not be connected to a running engine yet.
    // This is normal during bench development — the firmware
    // will keep trying to receive frames but won't crash.
    Serial.println("WARNING: No J1939 bus detected.");
    Serial.println("  (Plug into a running engine to receive data.)");
  }

  Serial.println("----------------------------------------");
}

// ============================================================
// loop()
// ============================================================
void loop() {
  // Read one CAN frame from the co-processor.
  // buf format: buf[0..2] = 24-bit PGN, buf[3..] = data payload.
  // receiveData() handles J1939 BAM multi-frame reassembly for DM1
  // messages that carry more than 2 active fault codes (>8 bytes).
  byte buf[128];
  int bytes = obd.receiveData(buf, sizeof(buf));

  if (bytes >= 3) {
    framesTotal++;

    uint32_t pgn = (uint32_t)buf[0] << 16 | (uint32_t)buf[1] << 8 | buf[2];
    trackUniquePgn(pgn);

    if (isKnownPgn(pgn)) {
      framesKnown++;
    } else {
      framesUnknown++;
    }

    // Decode the frame and update eng/faults
    dispatchPGN(buf, bytes, eng, faults);
  }

  // Periodic status print to serial
  if (millis() - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = millis();
    printEngineData();
  }

  // Check whether a debounced fault alert should fire
  checkFaultAlert();
}

// ============================================================
// printEngineData() — serial status dump
// Matches the expected output format from the project spec.
// ============================================================
void printEngineData() {
  Serial.println("\n--- Engine Data ---");

  if (!eng.valid) {
    Serial.println("  (waiting for PGN broadcasts...)");
  } else {
    if (eng.rpm >= 0) {
      Serial.print("  RPM:           ");
      Serial.println(eng.rpm, 1);
    }
    if (eng.coolantTempC > -999.0f) {
      Serial.print("  Coolant Temp:  ");
      Serial.print(eng.coolantTempC, 1);
      Serial.println(" C");
    }
    if (eng.vehicleSpeedKmh >= 0) {
      Serial.print("  Vehicle Speed: ");
      Serial.print(eng.vehicleSpeedKmh, 1);
      Serial.println(" km/h");
    }
    if (eng.batteryVoltage > 0) {
      Serial.print("  Battery:       ");
      Serial.print(eng.batteryVoltage, 2);
      Serial.println(" V");
    }
    if (eng.oilPressureKpa >= 0) {
      Serial.print("  Oil Pressure:  ");
      Serial.print(eng.oilPressureKpa, 0);
      Serial.println(" kPa");
    }
  }

  Serial.print("  [Frames: ");
  Serial.print(framesTotal);
  Serial.print(" | Known PGNs: ");
  Serial.print(framesKnown);
  Serial.print(" | Unknown: ");
  Serial.print(framesUnknown);
  Serial.print(" | Unique PGNs seen: ");
  Serial.print(seenPgnCount);
  Serial.println("]");
}

// ============================================================
// checkFaultAlert()
// DM1 is broadcast once per second while faults are active.
// Debounce it so we only alert once the fault has persisted for
// DM1_DEBOUNCE_MS rather than firing on a single noisy frame.
// ============================================================
void checkFaultAlert() {
  if (faults.count == 0) {
    // No active faults — reset debounce state
    faultPending     = false;
    faultFirstSeenMs = 0;
    return;
  }

  if (!faultPending) {
    // First time we're seeing a fault this cycle — start the timer
    faultPending     = true;
    faultFirstSeenMs = millis();
    return;
  }

  // Fault hasn't persisted long enough yet
  if (millis() - faultFirstSeenMs < DM1_DEBOUNCE_MS) return;

  // --- Fault is confirmed. Alert. ---
  Serial.println("\n!!! ACTIVE FAULT CODE(S) DETECTED !!!");
  for (int i = 0; i < faults.count; i++) {
    Serial.print("  DTC #");
    Serial.print(i + 1);
    Serial.print(": SPN ");
    Serial.print(faults.dtcs[i].spn);
    Serial.print(" / FMI ");
    Serial.print(faults.dtcs[i].fmi);
    Serial.print(" (count: ");
    Serial.print(faults.dtcs[i].occurrence);
    Serial.println(")");
  }

  // TODO Phase 1: Build JSON payload matching docs/payload_schema.json,
  //               POST to WEBHOOK_URL via SIM7600 cellular modem.
  // TODO Phase 2: If POST fails, write to SD_FAULT_FILE for store-and-forward.

  // Reset so the same fault doesn't retrigger every loop iteration.
  // NOTE: DM1 broadcasts ~1/sec while faults are active, so the same
  // fault WILL re-alert after another DM1_DEBOUNCE_MS window (~6s total).
  // For Phase 0 serial output this is fine — you want to see faults persist.
  // TODO Phase 1: Track which SPN/FMI combos have already been alerted and
  //   only re-alert when a NEW fault appears or occurrence count changes.
  //   Otherwise this will spam the mechanic with duplicate SMS.
  faults.count     = 0;
  faultPending     = false;
  faultFirstSeenMs = 0;
}

// ============================================================
// isKnownPgn() — returns true for PGNs we actively decode
// ============================================================
bool isKnownPgn(uint32_t pgn) {
  return (pgn == PGN_EEC1  || pgn == PGN_ET1   || pgn == PGN_EFLP1 ||
          pgn == PGN_CCVS  || pgn == PGN_VEP   || pgn == PGN_VD    ||
          pgn == PGN_AMB   || pgn == PGN_DM1);
}

// ============================================================
// trackUniquePgn() — logs first occurrence of each PGN seen
// Helps verify which PGNs the ECU actually broadcasts on this truck.
// ============================================================
void trackUniquePgn(uint32_t pgn) {
  for (int i = 0; i < seenPgnCount; i++) {
    if (seenPgns[i] == pgn) return; // Already seen
  }
  if (seenPgnCount < 64) {
    seenPgns[seenPgnCount++] = pgn;
  }
}
