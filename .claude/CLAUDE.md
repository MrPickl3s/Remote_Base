# Remote Base — Claude Code Context

## What This Project Is
Fleet telematics system for heavy-duty school buses (Cummins engines). Reads J1939/CAN bus data via Freematics ONE+ Model H (ESP32-based OBD-II dongle), transmits fault codes over LTE cellular (Hologram SIM) to a Google Apps Script webhook, which sends SMS alerts to mechanics via Twilio.

Core user story: mechanic gets a text like "Bus 07: SPN 3226 / FMI 0 (x3)" and knows what to prep before the bus arrives.

## Architecture (Phase 1-2)
```
[Cummins ECU] --J1939/CAN--> [Freematics ONE+ Model H] --LTE/HTTP POST--> [Google Apps Script] --Twilio--> [Mechanic's Phone]
```

## Hardware Constraints
- **Freematics ONE+ Model H**: ESP32, 16MB Flash, 8MB PSRAM, SIM7600A-H LTE modem, u-blox M10 GPS, microSD slot
- **J1939 is broadcast-only** on this device. Cannot send request/response messages to ECU. Passive listening only.
- **Cellular**: Hologram eUICC SIM, LTE bands B2/B4/B12, $0.03/MB pay-as-you-go
- **Low-power mode**: ~10mA with peripherals off. Wake on ignition (voltage threshold or accelerometer).

## Build Commands
```bash
# Firmware — compile for ESP32 (Arduino framework)
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/remote_base/

# Firmware — upload via USB
arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 firmware/remote_base/

# Cloud — Apps Script is deployed via clasp CLI
cd cloud/webhook
clasp push
```

## Project Structure
```
firmware/               C++ / Arduino framework
  remote_base/          Main firmware sketch (merged j1939_monitor + telelogger)
  data/                 SPN lookup table for microSD (Phase 3+, not Phase 1-2)
cloud/
  webhook/              Google Apps Script — receives POST, triggers Twilio SMS
config/
  fleet_config.json     Reference schema for the Google Sheet config
docs/
  payload_schema.json   Contract: what the dongle sends, what the webhook expects
```

## Key Technical Decisions
- **Phase 1-2: Raw fault codes only.** Firmware sends SPN/FMI numbers without human-readable descriptions. Mechanic interprets codes on his own device. No SPN lookup table needed on dongle or cloud side yet.
- **Phase 3: Cloud-side SPN lookup.** When we add descriptions, they live in Google Sheets or Supabase — NOT on the ESP32. Updating a cloud table is trivial; updating firmware on 50 trucks is not.
- **HTTP POST, not MQTT/UDP.** Simple HTTPS POST to Apps Script webhook URL. Reliable, debuggable, works with Google's infrastructure.
- **Store-and-forward is mandatory.** Rural Washington has coverage gaps. Fault events write to microSD when offline, POST on reconnection.

## Payload Schema (Phase 1-2)
See `docs/payload_schema.json` for the full spec. Summary:
```json
{
  "bus_id": "bus_07",
  "event_type": "fault",
  "timestamp": "2026-03-22T18:30:00Z",
  "faults": [
    { "spn": 3226, "fmi": 0, "occurrence": 3 }
  ],
  "meta": {
    "firmware_version": "0.1.0",
    "signal_strength": -85,
    "battery_voltage": 24.1
  }
}
```

## Non-Obvious Things You Need to Know
- **DM1 (PGN 65226) is variable-length.** If more than 2 active faults exist, the message exceeds 8 bytes and uses J1939 Transport Protocol (BAM). The firmware must handle multi-frame reassembly.
- **SIM7600 modem** is managed via AT commands through the Freematics library — not direct serial. Use `cell.httpSend()` style APIs, not raw AT commands.
- **Freematics library is a custom abstraction**, not standard ESP32 Arduino. CAN bus access goes through `FreematicsOBD` class methods, not ESP32 TWAI driver directly.
- **The device is NOT a real ELM327.** Do not use ELM327 AT command patterns.
- **Low-power mode**: `enterLowPowerMode()` from Freematics library. Wake sources: voltage change, accelerometer, or timer.

## Firmware Starting Point
Fork from Freematics GitHub repo (`stanleyhuangyc/Freematics`), `firmware_v5/` directory:
- `j1939_monitor.ino` — J1939 broadcast monitoring (CAN bus listening foundation)
- `telelogger/` — Full telemetry + cellular transmission (connectivity foundation)
Merge these into `firmware/remote_base/`. Do NOT write firmware from scratch.

## Developer Context
- Single developer, novice-to-intermediate, builds with AI assistance
- Available 10-15 hours/week
- AMD GPU (RX 6800 XT) — no CUDA, no NVIDIA tools
- Favor clarity over cleverness. Comment non-obvious code. Explain embedded concepts.

## Workflow
1. **Plan** in the Remote Base Claude Project (discussion, architecture decisions)
2. **Spec** gets written as a Claude Code handoff — specific files, steps, acceptance criteria
3. **Build** here in Claude Code — execute the spec
4. **Verify** — does it compile? Does mock data produce expected output? Does it work on real hardware?

## Scope Rules by Directory
- `firmware/**` — C++, Arduino framework, ESP32 memory-conscious, Freematics API only
- `cloud/**` — JavaScript (Google Apps Script), keep functions small and testable
- `config/**` — JSON schemas, documentation only
- `docs/**` — Specs, schemas, reference material
