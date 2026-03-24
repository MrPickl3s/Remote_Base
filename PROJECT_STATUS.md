# Remote Base — Project Status

## Last Updated: March 24, 2026

---

## Completed

### Infrastructure
- Google Apps Script webhook deployed and live
- Email-to-SMS confirmed working — test text received on mechanic's phone
- Webhook URL secured in private notes (do not share publicly)
- ramventure.pages.dev live on Cloudflare Pages (used for business verification)

### Accounts & Services
- Google Apps Script: deployed as web app, authorized
- Hologram SIM: ordered (ships from US, 3-5 days)
- Freematics ONE+ Model H: ordered (ships from Australia, 1-2 weeks)
- Twilio: account created, toll-free number acquired but NOT needed for Phase 1 — parked for Phase 4+
- ramventure.pages.dev: live, portfolio site with Remote Base, Scene Genie, Energy Codes Guide

### Phase 0 Firmware — COMPLETE
Three firmware files created and verified compiling cleanly:

**`firmware/src/config.h`** — Per-bus configuration (BUS_ID, WEBHOOK_URL, alert thresholds, timing constants). Includes HTTP timeout of 15s for rural latency. Fault dedup window of 30 minutes.

**`firmware/src/pgn_decoder.h`** — Decodes 9 key J1939 PGNs (EEC1, ET1, EFL/P1, CCVS, VEP, VD, AMB, DM1, DM2). DM1 parser extracts 19-bit SPN + 5-bit FMI from packed binary. All SPN byte math verified by Opus extended review.

**`firmware/src/main.cpp`** — Setup/loop, J1939 protocol init, serial display of decoded engine data every 5 seconds, DM1 fault detection with debounce. TODO placeholders for HTTP POST and cellular.

**Compile results:** RAM 8.2% (26,932 / 327,680 bytes), Flash 4.5% (295,425 / 6,553,600 bytes). Healthy headroom.

### Phase 0.5 — SPN Lookup + Payload Schema + Test Tooling (March 24)

**`firmware/src/spn_lookup.h`** — Hardcoded lookup table with 53 Cummins ISB/ISL school bus SPNs. Binary search lookup. Includes:
- Standard J1939 FMI description table (23 entries, FMI 0-31)
- Severity classification (RED / AMBER / INFO / NONE) per SPN
- Severity escalation logic: FMI 0/1 on critical parameters escalates AMBER → RED
- `buildFaultDescription()` generates human-readable strings for SMS
- Unknown SPNs handled gracefully: "Unknown Parameter (SPN 9999) - Voltage High"
- Table sort order verified by automated test
- ~3KB flash cost, zero PSRAM

**`firmware/src/payload.h`** — JSON payload builder for all 4 webhook event types:
- `fault` — DM1 fault code with SPN, FMI, occurrence count, severity, description
- `heartbeat` — uptime, cell RSSI, free heap, SD buffer count
- `checkin` — daily startup report with all engine params + GPS position
- `threshold_alert` — parameter name, value, threshold, direction, description
- Payload sizes: 152–251 bytes per event (~4,000 events per penny on Hologram)
- snprintf-based, no ArduinoJson dependency. Migration path noted for multi-fault DM1.

**`cloud/test_webhook.sh`** — Curl-based test script for webhook end-to-end testing:
- 8 test modes: fault, fault_red, fault_derate, fault_unknown, heartbeat, checkin, threshold, all, spam
- Realistic Cummins school bus fault scenarios (DEF low, coolant high, SCR derate)
- Spam mode sends 5 rapid faults to test dedup behavior
- Color-coded terminal output with HTTP status checking

**`firmware/src/compile_test.cpp`** — Desktop compile verification test:
- Verifies SPN table sort order (binary search requirement)
- Tests known/unknown SPN lookups
- Tests severity escalation logic
- Tests all 4 payload builders produce valid JSON
- All tests pass with g++ -std=c++11 -Wall -Wextra -Wpedantic

---

## Decisions Made

### SMS Delivery
- **Dropped Twilio for Phase 1** — A2P 10DLC registration costs ~$30 setup + $10/month recurring, not justified for one mechanic
- **Using email-to-SMS gateway instead** — free, works today, zero registration
- Mechanic is on **Red Pocket / Verizon network** → gateway: `NUMBER@vtext.com`
- **Twilio revisit trigger:** Phase 4+ when deploying to multiple districts with mechanics you haven't met

### Server Stack (confirmed)
- HTTP POST over LTE cellular (not MQTT, not UDP)
- Google Apps Script as webhook receiver
- MailApp.sendEmail() for SMS delivery (no Twilio dependency)
- No Traccar until GPS tracking is an explicit requirement

### Hologram / Twilio Clarification
- Hologram = SIM inside the dongle, carries data from bus to server. Unrelated to SMS.
- Twilio = SMS delivery layer. Replaced by email gateway for now.
- These are two completely separate data flows — do not conflate them.

### SPN Lookup Strategy (March 24)
- **Phase 1-2:** Hardcoded table in firmware (53 common Cummins SPNs). Binary search.
- **Phase 3:** Migrate to SD card JSON file. Same struct, loaded from file instead of compiled in.
- **SAE Digital Annex:** Not purchasing ($200-300). Open-source Cummins SPN databases sufficient for Phase 1-3.

### Payload Schema (March 24)
- JSON over HTTP POST. 4 event types: fault, heartbeat, checkin, threshold_alert.
- All payloads share common envelope: bus_id, event, timestamp, fw_version.
- Fault payloads include severity routing field (RED/AMBER/INFO/NONE).
- Payload sizes 152-251 bytes. Well within Hologram bandwidth budget.

### Phase 1 Priorities (March 24)
- **Before hardware arrives:** Build/update Apps Script to handle fault JSON payloads. Test end-to-end with curl script. Activate Hologram SIM when it arrives.
- **When hardware arrives:** Test with stock Freematics sketch FIRST (don't start with custom firmware). Verify BAM reassembly behavior. Then upload custom firmware.
- **Test harness:** NOT building. Better to test on real hardware. Time better spent on server side.
- **SD buffering:** Phase 2. Phase 1 adds SD logging only (append-only CSV, not store-and-forward).

---

## Next Actions (priority order)

### Before Hardware Arrives
1. Update Apps Script webhook to handle the 4 JSON event types (fault, heartbeat, checkin, threshold_alert)
2. Wire up SMS trigger for fault and threshold_alert events (email-to-SMS gateway)
3. Test end-to-end: run `test_webhook.sh all` → verify SMS arrives on mechanic's phone
4. Activate Hologram SIM when it arrives, verify on Hologram dashboard
5. Add firmware files to Remote_Base GitHub repo and push

### When Dongle Arrives (Phase 1: Hello World)
1. Insert SIM, connect dongle to laptop via USB
2. Upload stock Freematics `j1939_monitor.ino` — NOT custom firmware yet
3. Plug into test bus. Watch serial monitor. Document raw output.
4. **BAM verification:** Look for PGN 65226 frames. Single complete frames or fragmented? Document this.
5. If BAM works → proceed with custom firmware as-is
6. If BAM doesn't work → build BAM handler before anything else
7. Upload custom firmware. Verify decoded data matches dashboard gauges.
8. Add HTTP POST. Send first real fault data to webhook.

---

## Waiting On

| Item | ETA | Blocker? |
|---|---|---|
| Hologram SIM | ~3-5 days | Yes — needed for Phase 1 cellular test |
| Freematics ONE+ Model H | ~1-2 weeks | Yes — needed for all firmware testing |
| Mechanic test bus access | TBD | Coordinate with supervisor |

---

## Critical Unverified Assumption: BAM Reassembly

The firmware assumes the Freematics co-processor transparently handles multi-frame DM1 reassembly. If `obd.receiveData()` returns fully reassembled frames, current code works as-is. If it returns separate fragments, we need to build BAM protocol handling.

**This is binary — only real hardware can answer it.** When the dongle plugs into a running bus, the serial monitor reveals the truth immediately.

**Impact if assumption fails:** Adds ~1-2 days of development work to Phase 1. BAM handling becomes Priority 1 before cellular transmission.

---

## Architecture Note: Webhook URL Hardcoding

Current config.h has WEBHOOK_URL baked into firmware. On 1 bus, this is fine. On 15+ buses, changing the URL means reflashing every dongle.

**Phase 3 mitigation:** Add a redirect layer (Cloudflare Worker or similar). Dongle always POSTs to a fixed URL that forwards to current backend. One-line server change instead of fleet-wide reflash.

---

## Current Webhook Details

| Item | Value |
|---|---|
| Deployment | Google Apps Script Web App |
| Event types handled | fault, heartbeat, checkin, threshold_alert |
| SMS method | MailApp → carrier email gateway |
| Mechanic carrier | Verizon (Red Pocket MVNO) |
| Gateway format | `10digitnumber@vtext.com` |

---

## Current File Structure

```
firmware/
├── platformio.ini           (ESP32, 16MB flash, 921600 baud)
├── src/
│   ├── config.h            (BUS_ID, WEBHOOK_URL, thresholds, timing)
│   ├── pgn_decoder.h       (PGN decode logic + DM1 parser)
│   ├── spn_lookup.h        (53 Cummins SPNs, FMI table, binary search, severity)
│   ├── payload.h           (JSON builders for fault/heartbeat/checkin/threshold)
│   ├── main.cpp            (setup/loop, serial output, TODO: HTTP POST)
│   └── compile_test.cpp    (desktop verification — not deployed to hardware)
└── lib/
    └── FreematicsPlus/     (vendored from Freematics GitHub repo)

cloud/
├── test_webhook.sh         (curl-based webhook tester, 8 test modes)
└── (Apps Script code — deployed via Google Apps Script editor, not in repo)
```

---

## Roadmap Position

- ✅ Phase 0: Prep work — **COMPLETE**
- 🔄 Phase 0.5: SPN lookup + payload schema + test tooling — **COMPLETE**
- ⬜ Phase 1: Hello World (hardware connectivity) — **WAITING ON HARDWARE**
- ⬜ Phase 2: DM1 fault listener + SMS alert
- ⬜ Phase 3: Harden
- ⬜ Phase 4: Expand across fleet
- ⬜ Phase 5: Productize
- ⬜ Phase 6: First external district

---

## Open Questions / Things to Watch

- BAM reassembly: verified or not? (Phase 1 blocker — hardware dependent)
- Confirm mechanic's exact Verizon number and test gateway delivery with test_webhook.sh
- Regenerate Twilio Auth Token — was exposed in a document during prior session
- Consider adding a shared secret / API key to webhook before going to production (Phase 3 hardening task)
- Supervisor: confirm which test bus and when access is available
- Webhook URL hardcoding: plan redirect layer for Phase 3 before multi-bus deployment
