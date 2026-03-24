/**
 * compile_test.cpp — Verify spn_lookup.h and payload.h compile cleanly
 * 
 * This is NOT firmware. Just a desktop compile check.
 * Run with: g++ -std=c++11 -Wall -Wextra -o test compile_test.cpp && ./test
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

// Include our headers (they pull in config.h)
#include "spn_lookup.h"
#include "payload.h"

int main() {
    printf("=== Remote Base Header Compile Test ===\n\n");
    
    // --- Test 1: SPN Lookup ---
    printf("SPN Table: %u entries\n", SPN_TABLE_SIZE);
    
    // Verify table is sorted (binary search requirement)
    for (int i = 1; i < SPN_TABLE_SIZE; i++) {
        if (SPN_TABLE[i].spn <= SPN_TABLE[i-1].spn) {
            printf("ERROR: SPN table not sorted at index %d (SPN %u <= %u)\n",
                   i, SPN_TABLE[i].spn, SPN_TABLE[i-1].spn);
            return 1;
        }
    }
    printf("  Sort order: VERIFIED\n");
    
    // Test known SPNs
    const SpnEntry* e;
    
    e = lookupSpn(110);
    assert(e != nullptr);
    printf("  SPN 110: \"%s\" [%s]\n", e->description, severityToString(e->severity));
    assert(e->severity == SEVERITY_RED);
    
    e = lookupSpn(3226);
    assert(e != nullptr);
    printf("  SPN 3226: \"%s\" [%s]\n", e->description, severityToString(e->severity));
    
    e = lookupSpn(5246);
    assert(e != nullptr);
    printf("  SPN 5246: \"%s\" [%s]\n", e->description, severityToString(e->severity));
    assert(e->severity == SEVERITY_RED);
    
    // Test unknown SPN
    e = lookupSpn(99999);
    assert(e == nullptr);
    printf("  SPN 99999: (not found — correct)\n");
    
    // --- Test 2: FMI Lookup ---
    printf("\nFMI Table: %u entries\n", FMI_TABLE_SIZE);
    printf("  FMI 0: \"%s\"\n", lookupFmi(0));
    printf("  FMI 4: \"%s\"\n", lookupFmi(4));
    printf("  FMI 99: \"%s\"\n", lookupFmi(99));  // should be "Unknown FMI"
    
    // --- Test 3: Fault Description Builder ---
    char desc[128];
    buildFaultDescription(desc, sizeof(desc), 110, 0);
    printf("\nFault description (SPN 110, FMI 0): \"%s\"\n", desc);
    
    buildFaultDescription(desc, sizeof(desc), 9999, 3);
    printf("Fault description (SPN 9999, FMI 3): \"%s\"\n", desc);
    
    // --- Test 4: Severity Escalation ---
    printf("\nSeverity tests:\n");
    printf("  SPN 3226 FMI 18 (DEF low, moderate): %s\n", 
           severityToString(getFaultSeverity(3226, 18)));
    printf("  SPN 110 FMI 0 (coolant high, severe): %s\n",
           severityToString(getFaultSeverity(110, 0)));
    printf("  SPN 110 FMI 16 (coolant high, moderate): %s\n",
           severityToString(getFaultSeverity(110, 16)));
    printf("  SPN 9999 FMI 3 (unknown): %s\n",
           severityToString(getFaultSeverity(9999, 3)));
    
    // --- Test 5: Payload Builders ---
    char buf[PAYLOAD_BUF_SIZE];
    int len;
    
    printf("\n--- Fault Payload ---\n");
    len = buildFaultPayload(buf, sizeof(buf), 3226, 18, 1, 1711234567UL);
    printf("%s\n(%d bytes)\n", buf, len);
    
    printf("\n--- Heartbeat Payload ---\n");
    len = buildHeartbeatPayload(buf, sizeof(buf), 1711234567UL, 14400, -67, 245000, 0);
    printf("%s\n(%d bytes)\n", buf, len);
    
    printf("\n--- Checkin Payload ---\n");
    len = buildCheckinPayload(buf, sizeof(buf), 1711234567UL,
                               650.5f, 78.0f, 310.0f, 13.8f, 0.0f, 12.5f,
                               145320, 8450, 47.6062f, -122.3321f);
    printf("%s\n(%d bytes)\n", buf, len);
    
    printf("\n--- Threshold Payload ---\n");
    len = buildThresholdPayload(buf, sizeof(buf), 1711234567UL,
                                 "coolant_c", 108.5f, 104.0f, true);
    printf("%s\n(%d bytes)\n", buf, len);
    
    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}
