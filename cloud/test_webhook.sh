#!/bin/bash
# ============================================================================
# test_webhook.sh — Test Remote Base webhook with realistic payloads
# 
# Remote Base Fleet Telematics — Phase 1
#
# Usage:
#   1. Set your webhook URL below (from Google Apps Script deployment)
#   2. chmod +x test_webhook.sh
#   3. ./test_webhook.sh fault         # Send a single fault event
#   4. ./test_webhook.sh heartbeat     # Send a heartbeat
#   5. ./test_webhook.sh checkin       # Send a daily checkin
#   6. ./test_webhook.sh threshold     # Send a threshold alert
#   7. ./test_webhook.sh all           # Send one of each
#   8. ./test_webhook.sh spam          # Send 5 rapid faults (test dedup)
#
# The webhook should respond with HTTP 200. If SMS is wired up,
# you should get a text within 60 seconds for fault/threshold events.
# ============================================================================

# ---- CONFIGURE THIS ----
WEBHOOK_URL="YOUR_APPS_SCRIPT_URL_HERE"
# Example: https://script.google.com/macros/s/AKfycbw.../exec
# ---- END CONFIG ----

# Timestamp (Unix epoch seconds)
TS=$(date +%s)

# Colors for terminal output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

send_payload() {
    local label="$1"
    local payload="$2"
    
    echo -e "${YELLOW}--- Sending: ${label} ---${NC}"
    echo "$payload" | python3 -m json.tool 2>/dev/null || echo "$payload"
    echo ""
    
    HTTP_CODE=$(curl -s -o /tmp/webhook_response.txt -w "%{http_code}" \
        -X POST \
        -H "Content-Type: application/json" \
        -d "$payload" \
        "$WEBHOOK_URL")
    
    RESPONSE=$(cat /tmp/webhook_response.txt)
    
    if [ "$HTTP_CODE" = "200" ] || [ "$HTTP_CODE" = "302" ]; then
        echo -e "${GREEN}✓ HTTP $HTTP_CODE — Success${NC}"
    else
        echo -e "${RED}✗ HTTP $HTTP_CODE — Failed${NC}"
    fi
    
    if [ -n "$RESPONSE" ]; then
        echo "Response: $RESPONSE"
    fi
    echo ""
}

# ============================================================================
# TEST PAYLOADS — realistic Cummins school bus scenarios
# ============================================================================

# FAULT: DEF Tank Level Low — the most common school bus alert
FAULT_DEF='{
  "bus_id": "BUS-007",
  "event": "fault",
  "timestamp": '$TS',
  "fw_version": "0.1.0",
  "spn": 3226,
  "fmi": 18,
  "oc": 1,
  "severity": "AMBER",
  "description": "AFT SCR DEF Tank Level - Below Normal (Moderate)"
}'

# FAULT: Coolant Temp High — RED severity, send the tow truck
FAULT_COOLANT='{
  "bus_id": "BUS-007",
  "event": "fault",
  "timestamp": '$TS',
  "fw_version": "0.1.0",
  "spn": 110,
  "fmi": 0,
  "oc": 2,
  "severity": "RED",
  "description": "Engine Coolant Temperature - Above Normal (Severe)"
}'

# FAULT: SCR Inducement Derate — bus is going to 5mph, critical
FAULT_DERATE='{
  "bus_id": "BUS-007",
  "event": "fault",
  "timestamp": '$TS',
  "fw_version": "0.1.0",
  "spn": 5246,
  "fmi": 0,
  "oc": 1,
  "severity": "RED",
  "description": "AFT SCR Inducement (Severe Derate) - Above Normal (Severe)"
}'

# FAULT: Unknown SPN — tests the "unknown parameter" fallback
FAULT_UNKNOWN='{
  "bus_id": "BUS-007",
  "event": "fault",
  "timestamp": '$TS',
  "fw_version": "0.1.0",
  "spn": 9999,
  "fmi": 3,
  "oc": 1,
  "severity": "AMBER",
  "description": "Unknown Parameter (SPN 9999) - Voltage High / Shorted High"
}'

# HEARTBEAT: everything normal
HEARTBEAT='{
  "bus_id": "BUS-007",
  "event": "heartbeat",
  "timestamp": '$TS',
  "fw_version": "0.1.0",
  "uptime_s": 14400,
  "cell_rssi": -67,
  "free_heap": 245000,
  "sd_buffered": 0
}'

# CHECKIN: morning startup report
CHECKIN='{
  "bus_id": "BUS-007",
  "event": "checkin",
  "timestamp": '$TS',
  "fw_version": "0.1.0",
  "rpm": 650.5,
  "coolant_c": 78.0,
  "oil_kpa": 310.0,
  "battery_v": 13.8,
  "speed_kph": 0.0,
  "ambient_c": 12.5,
  "distance_km": 145320,
  "engine_hrs": 8450,
  "lat": 47.6062,
  "lon": -122.3321
}'

# THRESHOLD: coolant temp crossed limit
THRESHOLD='{
  "bus_id": "BUS-007",
  "event": "threshold_alert",
  "timestamp": '$TS',
  "fw_version": "0.1.0",
  "parameter": "coolant_c",
  "value": 108.5,
  "threshold": 104.0,
  "direction": "above",
  "description": "coolant_c 108.5 exceeds limit 104.0"
}'

# ============================================================================
# DISPATCH
# ============================================================================

if [ "$WEBHOOK_URL" = "YOUR_APPS_SCRIPT_URL_HERE" ]; then
    echo -e "${RED}ERROR: Set your WEBHOOK_URL at the top of this script first.${NC}"
    echo "Open test_webhook.sh and replace YOUR_APPS_SCRIPT_URL_HERE with your"
    echo "Google Apps Script deployment URL."
    exit 1
fi

case "${1:-all}" in
    fault)
        send_payload "FAULT: DEF Tank Low" "$FAULT_DEF"
        ;;
    fault_red)
        send_payload "FAULT: Coolant Temp HIGH (RED)" "$FAULT_COOLANT"
        ;;
    fault_derate)
        send_payload "FAULT: SCR Derate (RED)" "$FAULT_DERATE"
        ;;
    fault_unknown)
        send_payload "FAULT: Unknown SPN" "$FAULT_UNKNOWN"
        ;;
    heartbeat)
        send_payload "HEARTBEAT" "$HEARTBEAT"
        ;;
    checkin)
        send_payload "CHECKIN: Morning startup" "$CHECKIN"
        ;;
    threshold)
        send_payload "THRESHOLD: Coolant high" "$THRESHOLD"
        ;;
    all)
        send_payload "FAULT: DEF Tank Low" "$FAULT_DEF"
        sleep 2
        send_payload "HEARTBEAT" "$HEARTBEAT"
        sleep 2
        send_payload "CHECKIN: Morning startup" "$CHECKIN"
        sleep 2
        send_payload "THRESHOLD: Coolant high" "$THRESHOLD"
        ;;
    spam)
        echo -e "${YELLOW}Sending 5 rapid fault events to test deduplication...${NC}"
        echo ""
        for i in 1 2 3 4 5; do
            send_payload "FAULT #$i: DEF Tank Low (rapid fire)" "$FAULT_DEF"
            sleep 1
        done
        echo -e "${YELLOW}If you got 5 SMS messages, dedup is NOT working yet.${NC}"
        echo -e "${YELLOW}Phase 2 should limit this to 1 SMS per SPN per 30 minutes.${NC}"
        ;;
    *)
        echo "Usage: $0 {fault|fault_red|fault_derate|fault_unknown|heartbeat|checkin|threshold|all|spam}"
        exit 1
        ;;
esac

echo -e "${GREEN}Done.${NC}"
