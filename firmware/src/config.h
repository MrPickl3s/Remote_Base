#ifndef CONFIG_H
#define CONFIG_H

/**
 * config.h — Remote Base Firmware Configuration
 * 
 * Per-bus settings. Modify these before flashing each dongle.
 * Phase 3 goal: pull these from Google Sheet on boot instead of hardcoding.
 */

// ---- Bus Identity ----
#define BUS_ID              "BUS-007"
#define DISTRICT_ID         "DISTRICT-01"

// ---- Webhook ----
#define WEBHOOK_URL         "https://script.google.com/macros/s/YOUR_SCRIPT_ID/exec"

// ---- Cellular (Hologram) ----
#define APN                 "hologram"

// ---- Alert Thresholds ----
#define COOLANT_TEMP_WARN_C     104.0f    // °C — alert above this
#define COOLANT_TEMP_CRIT_C     110.0f    // °C — RED alert above this  
#define BATTERY_VOLT_LOW_V      11.8f     // V — alert below during cranking
#define BATTERY_VOLT_CRIT_V     11.0f     // V — RED alert
#define DEF_LEVEL_LOW_PCT       15.0f     // % — alert below this
#define OIL_PRESSURE_LOW_KPA    100.0f    // kPa — alert below this

// ---- Timing ----
#define HEARTBEAT_INTERVAL_MS   (4UL * 60UL * 60UL * 1000UL)  // 4 hours
#define CHECKIN_INTERVAL_MS     (24UL * 60UL * 60UL * 1000UL)  // 24 hours
#define FAULT_DEDUP_MS          (30UL * 60UL * 1000UL)          // 30 min dedup window
#define SERIAL_PRINT_INTERVAL   5000                             // 5 sec serial output
#define HTTP_TIMEOUT_MS         15000                            // 15 sec (rural latency)

// ---- Hardware Pins (Freematics Model H defaults) ----
// These are managed by the FreematicsPlus library; listed here for reference.
// #define PIN_SD_CS  5     // microSD chip select (SPI)

#endif // CONFIG_H
