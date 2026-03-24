#ifndef PAYLOAD_H
#define PAYLOAD_H

/**
 * payload.h — JSON Payload Builder for Remote Base Webhook
 * 
 * Remote Base Fleet Telematics — Phase 1
 * 
 * Builds JSON strings for HTTP POST to Google Apps Script webhook.
 * All payloads share a common envelope, with event-specific fields.
 * 
 * Design decisions:
 *   - snprintf-based string building, not ArduinoJson. Keeps binary small
 *     and avoids a library dependency for straightforward flat JSON.
 *   - Fixed buffer size (512 bytes). Worst case DM1 with long description
 *     fits in ~300 bytes. 512 gives headroom without wasting SRAM.
 *   - If you ever need nested objects or arrays (multi-fault DM1), switch
 *     to ArduinoJson. For Phase 1 single-fault payloads, this is fine.
 * 
 * Payload Schema (all events):
 * {
 *   "bus_id":      "BUS-007",          // from config.h
 *   "event":       "fault",            // fault | heartbeat | checkin | threshold_alert
 *   "timestamp":   1711234567,         // Unix epoch seconds (from GPS or NTP)
 *   "fw_version":  "0.1.0",           // firmware version string
 *   ... event-specific fields ...
 * }
 */

#include <stdio.h>
#include <stdint.h>
#include "config.h"
#include "spn_lookup.h"

// Buffer size for JSON payloads. 512 bytes handles all current event types.
// DM1 fault payload with max-length description: ~300 bytes.
// Checkin with all parameters: ~400 bytes.
#define PAYLOAD_BUF_SIZE 512

// Firmware version — bump this with each flash
#define FW_VERSION "0.1.0"


// ============================================================================
// FAULT EVENT — DM1 fault code detected
// ============================================================================
// Triggered when: PGN 65226 decoded with active fault
// SMS behavior:   RED severity = immediate SMS. AMBER = SMS. INFO = log only.
//
// Example:
// {
//   "bus_id": "BUS-007",
//   "event": "fault",
//   "timestamp": 1711234567,
//   "fw_version": "0.1.0",
//   "spn": 110,
//   "fmi": 0,
//   "oc": 3,
//   "severity": "RED",
//   "description": "Engine Coolant Temperature - Above Normal (Severe)"
// }

static int buildFaultPayload(char* buf, size_t bufSize,
                              uint32_t spn, uint8_t fmi, uint8_t occurrenceCount,
                              unsigned long timestamp) {
    // Build the human-readable description
    char desc[128];
    buildFaultDescription(desc, sizeof(desc), spn, fmi);
    
    // Get severity for routing
    AlertSeverity sev = getFaultSeverity(spn, fmi);
    
    return snprintf(buf, bufSize,
        "{"
        "\"bus_id\":\"%s\","
        "\"event\":\"fault\","
        "\"timestamp\":%lu,"
        "\"fw_version\":\"%s\","
        "\"spn\":%lu,"
        "\"fmi\":%u,"
        "\"oc\":%u,"
        "\"severity\":\"%s\","
        "\"description\":\"%s\""
        "}",
        BUS_ID,
        timestamp,
        FW_VERSION,
        (unsigned long)spn,
        (unsigned)fmi,
        (unsigned)occurrenceCount,
        severityToString(sev),
        desc
    );
}


// ============================================================================
// HEARTBEAT EVENT — periodic "I'm alive" ping
// ============================================================================
// Triggered when: Every HEARTBEAT_INTERVAL_MS (default 4 hours)
// SMS behavior:   Never triggers SMS. Server uses absence of heartbeat to
//                 detect offline dongles.
//
// Example:
// {
//   "bus_id": "BUS-007",
//   "event": "heartbeat",
//   "timestamp": 1711234567,
//   "fw_version": "0.1.0",
//   "uptime_s": 14400,
//   "cell_rssi": -67,
//   "free_heap": 245000,
//   "sd_buffered": 0
// }

static int buildHeartbeatPayload(char* buf, size_t bufSize,
                                  unsigned long timestamp,
                                  unsigned long uptimeSeconds,
                                  int cellRssi,
                                  uint32_t freeHeap,
                                  uint16_t sdBufferedEvents) {
    return snprintf(buf, bufSize,
        "{"
        "\"bus_id\":\"%s\","
        "\"event\":\"heartbeat\","
        "\"timestamp\":%lu,"
        "\"fw_version\":\"%s\","
        "\"uptime_s\":%lu,"
        "\"cell_rssi\":%d,"
        "\"free_heap\":%lu,"
        "\"sd_buffered\":%u"
        "}",
        BUS_ID,
        timestamp,
        FW_VERSION,
        uptimeSeconds,
        cellRssi,
        (unsigned long)freeHeap,
        (unsigned)sdBufferedEvents
    );
}


// ============================================================================
// CHECKIN EVENT — daily status report at ignition-on
// ============================================================================
// Triggered when: First ignition-on detected each day
// SMS behavior:   No SMS unless configured. Logged for fleet reporting.
//
// Example:
// {
//   "bus_id": "BUS-007",
//   "event": "checkin",
//   "timestamp": 1711234567,
//   "fw_version": "0.1.0",
//   "rpm": 650.5,
//   "coolant_c": 78.0,
//   "oil_kpa": 310.0,
//   "battery_v": 13.8,
//   "speed_kph": 0.0,
//   "ambient_c": 12.5,
//   "distance_km": 145320,
//   "engine_hrs": 8450,
//   "lat": 47.6062,
//   "lon": -122.3321
// }

static int buildCheckinPayload(char* buf, size_t bufSize,
                                unsigned long timestamp,
                                float rpm, float coolantC, float oilKpa,
                                float batteryV, float speedKph, float ambientC,
                                uint32_t distanceKm, uint32_t engineHrs,
                                float lat, float lon) {
    return snprintf(buf, bufSize,
        "{"
        "\"bus_id\":\"%s\","
        "\"event\":\"checkin\","
        "\"timestamp\":%lu,"
        "\"fw_version\":\"%s\","
        "\"rpm\":%.1f,"
        "\"coolant_c\":%.1f,"
        "\"oil_kpa\":%.1f,"
        "\"battery_v\":%.2f,"
        "\"speed_kph\":%.1f,"
        "\"ambient_c\":%.1f,"
        "\"distance_km\":%lu,"
        "\"engine_hrs\":%lu,"
        "\"lat\":%.6f,"
        "\"lon\":%.6f"
        "}",
        BUS_ID,
        timestamp,
        FW_VERSION,
        rpm, coolantC, oilKpa,
        batteryV, speedKph, ambientC,
        (unsigned long)distanceKm,
        (unsigned long)engineHrs,
        lat, lon
    );
}


// ============================================================================
// THRESHOLD ALERT EVENT — parameter crossed configured limit
// ============================================================================
// Triggered when: A monitored parameter exceeds its threshold in config.h
// SMS behavior:   Always triggers SMS. These are configured by the mechanic.
//
// Example:
// {
//   "bus_id": "BUS-007",
//   "event": "threshold_alert",
//   "timestamp": 1711234567,
//   "fw_version": "0.1.0",
//   "parameter": "coolant_c",
//   "value": 108.5,
//   "threshold": 104.0,
//   "direction": "above",
//   "description": "Engine Coolant Temperature 108.5°C exceeds limit 104.0°C"
// }

static int buildThresholdPayload(char* buf, size_t bufSize,
                                  unsigned long timestamp,
                                  const char* parameter,
                                  float value, float threshold,
                                  bool isAbove) {
    // Build readable description
    char desc[128];
    snprintf(desc, sizeof(desc), "%s %.1f %s limit %.1f",
             parameter, value,
             isAbove ? "exceeds" : "below",
             threshold);
    
    return snprintf(buf, bufSize,
        "{"
        "\"bus_id\":\"%s\","
        "\"event\":\"threshold_alert\","
        "\"timestamp\":%lu,"
        "\"fw_version\":\"%s\","
        "\"parameter\":\"%s\","
        "\"value\":%.2f,"
        "\"threshold\":%.2f,"
        "\"direction\":\"%s\","
        "\"description\":\"%s\""
        "}",
        BUS_ID,
        timestamp,
        FW_VERSION,
        parameter,
        value, threshold,
        isAbove ? "above" : "below",
        desc
    );
}

#endif // PAYLOAD_H
