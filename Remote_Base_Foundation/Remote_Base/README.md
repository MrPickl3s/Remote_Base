# Remote Base

Fleet telematics system for heavy-duty school buses. Reads real-time engine fault codes via J1939/CAN bus and sends SMS alerts to mechanics.

## How It Works

A Freematics ONE+ Model H dongle plugs into the bus diagnostic port and passively listens to J1939 broadcast traffic from the Cummins ECU. When a fault code appears (DM1 — Active Diagnostic Trouble Codes), the dongle sends the raw SPN/FMI data over LTE cellular to a Google Apps Script webhook, which fires an SMS to the assigned mechanic via Twilio.

The mechanic gets a text like:

> **Bus 07:** SPN 3226 / FMI 0 (x3)

He enters the code into his diagnostic reference and knows what to prep before the bus arrives — or whether to send a tow truck instead.

## Architecture

```
[Cummins ECU] → J1939/CAN → [Freematics ONE+ Model H] → LTE/HTTP POST → [Google Apps Script] → Twilio → [Mechanic SMS]
```

## Hardware

- **Freematics ONE+ Model H** — ESP32 OBD-II dongle, native 24V heavy-duty support, LTE cellular, GPS, microSD
- **Hologram Hyper eUICC SIM** — Pay-as-you-go LTE ($0.03/MB)
- **SAE J1939** — Broadcast-only monitoring (passive CAN bus listening)

## Project Structure

```
firmware/           ESP32 firmware (C++ / Arduino)
cloud/              Google Apps Script webhook + Twilio SMS
config/             Fleet configuration schema
docs/               Payload schemas and specs
```

## Status

**Phase 0 — Foundation.** Repo structure, schemas, and development workflow established. Hardware on order.

## License

Proprietary. All rights reserved.
