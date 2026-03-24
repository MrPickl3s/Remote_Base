#ifndef SPN_LOOKUP_H
#define SPN_LOOKUP_H

/**
 * spn_lookup.h — Hardcoded SPN/FMI Lookup for Cummins School Bus Engines
 * 
 * Remote Base Fleet Telematics — Phase 1
 * 
 * Contains:
 *   1. ~40 most common Cummins ISB/ISL SPNs seen on school buses
 *   2. Standard J1939 FMI descriptions (0-31)
 *   3. Binary search lookup function (table is sorted by SPN)
 *   4. Severity classification (RED/AMBER/NONE) for alert routing
 * 
 * Design decisions:
 *   - Hardcoded in flash, not loaded from SD. Costs ~3KB flash, zero PSRAM.
 *   - Sorted array + binary search = O(log n) lookup, no hash table overhead.
 *   - Unknown SPNs return "Unknown Parameter" — still useful with raw SPN number.
 *   - Migration path to SD-based JSON is clean: same struct, different loader.
 * 
 * Sources: Cummins Tier 4 fault code references, J1939 standard FMI table,
 *          ISB 6.7 common fault lists from fleet maintenance communities.
 * 
 * To add a new SPN: insert in sorted order by spn number. The binary search
 * depends on the array being sorted.
 */

#include <stdint.h>

// ============================================================================
// SEVERITY LEVELS — used for alert routing decisions
// ============================================================================

enum AlertSeverity : uint8_t {
    SEVERITY_RED    = 0,  // Stop engine / tow truck — immediate SMS
    SEVERITY_AMBER  = 1,  // Warning — SMS with normal priority
    SEVERITY_INFO   = 2,  // Informational — log only, no SMS unless configured
    SEVERITY_NONE   = 3   // No action needed
};

// ============================================================================
// SPN ENTRY STRUCT
// ============================================================================

struct SpnEntry {
    uint32_t spn;
    const char* description;       // Short human-readable name
    AlertSeverity severity;        // Default severity (FMI can override)
};

// ============================================================================
// SPN TABLE — SORTED BY SPN NUMBER (required for binary search)
// ============================================================================
// Focus: Cummins ISB 6.7 / ISL 9 school bus engines
// Covers: engine critical, aftertreatment/DEF/SCR/DPF, EGR, electrical, fuel
// ============================================================================

static const SpnEntry SPN_TABLE[] = {
    // --- Engine Core ---
    {  22,  "Crankcase Blow-by Pressure",            SEVERITY_AMBER },
    {  27,  "EGR Valve Position",                    SEVERITY_AMBER },
    {  84,  "Vehicle Speed Sensor",                  SEVERITY_AMBER },
    {  91,  "Accelerator Pedal Position Sensor",     SEVERITY_AMBER },
    {  94,  "Fuel Delivery Pressure",                SEVERITY_AMBER },
    { 100,  "Engine Oil Pressure",                   SEVERITY_RED   },
    { 102,  "Intake Manifold Pressure",              SEVERITY_AMBER },
    { 105,  "Intake Manifold Temperature",           SEVERITY_AMBER },
    { 107,  "Air Filter Differential Pressure",      SEVERITY_AMBER },
    { 110,  "Engine Coolant Temperature",            SEVERITY_RED   },
    { 111,  "Coolant Level",                         SEVERITY_RED   },
    { 157,  "Fuel Rail Pressure",                    SEVERITY_AMBER },
    { 168,  "Battery Voltage",                       SEVERITY_AMBER },
    { 171,  "Ambient Air Temperature",               SEVERITY_INFO  },
    { 174,  "Fuel Temperature",                      SEVERITY_AMBER },
    { 175,  "Engine Oil Temperature",                SEVERITY_AMBER },
    { 190,  "Engine Speed (RPM)",                    SEVERITY_INFO  },

    // --- Aftertreatment / DEF / SCR / DPF ---
    { 411,  "EGR Valve Delta Pressure",              SEVERITY_AMBER },
    { 412,  "EGR Temperature",                       SEVERITY_AMBER },
    { 520,  "Coolant Pressure",                      SEVERITY_AMBER },
    { 612,  "Crankshaft Speed/Position Signal",      SEVERITY_RED   },
    { 629,  "Engine Control Module (ECM)",           SEVERITY_RED   },
    { 651,  "Injector Cylinder #1",                  SEVERITY_AMBER },
    { 652,  "Injector Cylinder #2",                  SEVERITY_AMBER },
    { 653,  "Injector Cylinder #3",                  SEVERITY_AMBER },
    { 654,  "Injector Cylinder #4",                  SEVERITY_AMBER },
    { 655,  "Injector Cylinder #5",                  SEVERITY_AMBER },
    { 656,  "Injector Cylinder #6",                  SEVERITY_AMBER },
    {1569,  "Engine Protection Torque Derate",       SEVERITY_RED   },
    {3031,  "Aftertreatment DPF Soot Load",          SEVERITY_AMBER },
    {3216,  "AFT DPF Ash Load Percent",              SEVERITY_AMBER },
    {3226,  "AFT SCR DEF Tank Level",                SEVERITY_AMBER },
    {3242,  "AFT DPF Regen Needed",                  SEVERITY_AMBER },
    {3251,  "AFT DPF Differential Pressure",         SEVERITY_AMBER },
    {3361,  "AFT SCR NOx Conversion Efficiency",     SEVERITY_AMBER },
    {3363,  "AFT SCR DEF Dosing Valve",              SEVERITY_AMBER },
    {3364,  "AFT SCR DEF Pump",                      SEVERITY_AMBER },
    {3543,  "NOx Limit Exceeded / DEF Quality",      SEVERITY_RED   },
    {3556,  "AFT DEF Tank Temperature",              SEVERITY_AMBER },
    {3582,  "AFT SCR Conversion Efficiency Low",     SEVERITY_RED   },
    {3597,  "Engine Coolant Diversion Valve",        SEVERITY_AMBER },
    {3609,  "AFT DPF Outlet Pressure",               SEVERITY_AMBER },
    {3712,  "AFT SCR Operator Inducement",           SEVERITY_RED   },
    {3714,  "Engine Protection Torque Derate",       SEVERITY_RED   },
    {3720,  "AFT DPF Soot High",                     SEVERITY_AMBER },
    {4334,  "AFT DEF Pump Motor Speed",              SEVERITY_AMBER },
    {4364,  "AFT SCR Catalyst Conversion",           SEVERITY_AMBER },
    {4765,  "AFT DEF Tank Heater",                   SEVERITY_AMBER },
    {5246,  "AFT SCR Inducement (Severe Derate)",    SEVERITY_RED   },
    {5394,  "AFT Hydrocarbon Doser",                 SEVERITY_AMBER },
    {5742,  "AFT DPF Regen Frequency",               SEVERITY_AMBER },
    {6254,  "AFT Derate - Shutdown Warning",         SEVERITY_RED   },
    {6255,  "AFT Derate - Speed Limited",            SEVERITY_RED   },
};

static const uint16_t SPN_TABLE_SIZE = sizeof(SPN_TABLE) / sizeof(SPN_TABLE[0]);


// ============================================================================
// FMI TABLE — Standard J1939 Failure Mode Identifiers (0-31)
// ============================================================================
// These are universal across ALL J1939 engines, not Cummins-specific.
// ============================================================================

struct FmiEntry {
    uint8_t fmi;
    const char* description;
};

static const FmiEntry FMI_TABLE[] = {
    {  0, "Above Normal (Severe)"         },
    {  1, "Below Normal (Severe)"         },
    {  2, "Data Erratic/Intermittent"     },
    {  3, "Voltage High / Shorted High"   },
    {  4, "Voltage Low / Shorted Low"     },
    {  5, "Current Low / Open Circuit"    },
    {  6, "Current High / Grounded"       },
    {  7, "Mechanical Not Responding"     },
    {  8, "Abnormal Frequency"            },
    {  9, "Abnormal Update Rate"          },
    { 10, "Abnormal Rate of Change"       },
    { 11, "Root Cause Unknown"            },
    { 12, "Bad Device or Component"       },
    { 13, "Out of Calibration"            },
    { 14, "Special Instructions"          },
    { 15, "Above Normal (Least Severe)"   },
    { 16, "Above Normal (Moderate)"       },
    { 17, "Below Normal (Least Severe)"   },
    { 18, "Below Normal (Moderate)"       },
    { 19, "Received Network Data Error"   },
    { 20, "Reserved"                      },
    { 21, "Reserved"                      },
    { 31, "Condition Exists"              },
};

static const uint8_t FMI_TABLE_SIZE = sizeof(FMI_TABLE) / sizeof(FMI_TABLE[0]);


// ============================================================================
// LOOKUP FUNCTIONS
// ============================================================================

/**
 * Binary search for SPN description.
 * Returns pointer to SpnEntry if found, nullptr if not.
 * Table MUST be sorted by spn for this to work.
 */
static const SpnEntry* lookupSpn(uint32_t spn) {
    int low = 0;
    int high = SPN_TABLE_SIZE - 1;
    
    while (low <= high) {
        int mid = (low + high) / 2;
        if (SPN_TABLE[mid].spn == spn) {
            return &SPN_TABLE[mid];
        } else if (SPN_TABLE[mid].spn < spn) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return nullptr;  // SPN not in our table
}

/**
 * Get FMI description string.
 * Returns "Unknown FMI" for values not in table.
 */
static const char* lookupFmi(uint8_t fmi) {
    for (uint8_t i = 0; i < FMI_TABLE_SIZE; i++) {
        if (FMI_TABLE[i].fmi == fmi) {
            return FMI_TABLE[i].description;
        }
    }
    return "Unknown FMI";
}

/**
 * Get severity string for JSON payload and SMS routing.
 */
static const char* severityToString(AlertSeverity sev) {
    switch (sev) {
        case SEVERITY_RED:   return "RED";
        case SEVERITY_AMBER: return "AMBER";
        case SEVERITY_INFO:  return "INFO";
        default:             return "NONE";
    }
}

/**
 * Build a human-readable fault description string.
 * Writes to the provided buffer. Returns number of chars written.
 * 
 * Example output: "Engine Coolant Temperature - Above Normal (Severe)"
 * Unknown SPN:    "Unknown Parameter (SPN 9999) - Voltage High / Shorted High"
 */
static int buildFaultDescription(char* buf, size_t bufSize, uint32_t spn, uint8_t fmi) {
    const SpnEntry* entry = lookupSpn(spn);
    const char* fmiDesc = lookupFmi(fmi);
    
    if (entry) {
        return snprintf(buf, bufSize, "%s - %s", entry->description, fmiDesc);
    } else {
        return snprintf(buf, bufSize, "Unknown Parameter (SPN %lu) - %s", 
                        (unsigned long)spn, fmiDesc);
    }
}

/**
 * Determine if a fault should trigger an SMS alert.
 * Uses SPN severity as baseline. FMI 0 or 1 (severe range violations)
 * can escalate AMBER faults to RED.
 */
static AlertSeverity getFaultSeverity(uint32_t spn, uint8_t fmi) {
    const SpnEntry* entry = lookupSpn(spn);
    
    if (!entry) {
        // Unknown SPN: alert as AMBER so the mechanic sees it
        // but don't treat it as RED (could be noise)
        return SEVERITY_AMBER;
    }
    
    AlertSeverity base = entry->severity;
    
    // FMI 0 or 1 = "most severe level" range violation
    // Escalate AMBER parameters to RED if the condition is severe
    if ((fmi == 0 || fmi == 1) && base == SEVERITY_AMBER) {
        // Engine-critical parameters escalate on severe FMI
        if (spn == 100 || spn == 110 || spn == 111 || spn == 102 || 
            spn == 157 || spn == 612) {
            return SEVERITY_RED;
        }
    }
    
    return base;
}

#endif // SPN_LOOKUP_H
