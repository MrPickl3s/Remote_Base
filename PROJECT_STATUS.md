# Remote Base — Project Status

## Last Updated: March 23, 2026

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

---

## Next Action (do this first next session)

**Get the firmware dev environment ready while hardware ships:**

1. Install Arduino IDE 2.x on laptop
2. Clone Freematics repo: `git clone https://github.com/stanleyhuangyc/Freematics.git`
3. Install FreematicsPlus library in Arduino IDE
4. Open `firmware_v5/j1939_monitor/j1939_monitor.ino`
5. Verify it compiles without errors (no hardware needed for this step)

Goal: when the dongle arrives, you're not setting up software — you're plugging in and testing.

---

## Waiting On

| Item | ETA | Blocker? |
|---|---|---|
| Hologram SIM | ~3-5 days | Yes — needed for Phase 1 cellular test |
| Freematics ONE+ Model H | ~1-2 weeks | Yes — needed for all firmware testing |
| Mechanic test bus access | TBD | Coordinate with supervisor |

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

## Roadmap Position

- ✅ Phase 0: Prep work — **IN PROGRESS**
- ⬜ Phase 1: Hello World (hardware connectivity)
- ⬜ Phase 2: DM1 fault listener + SMS alert
- ⬜ Phase 3: Harden
- ⬜ Phase 4: Expand across fleet
- ⬜ Phase 5: Productize
- ⬜ Phase 6: First external district

---

## Open Questions / Things to Watch

- Confirm mechanic's exact Verizon number and test gateway delivery again once Apps Script is fully deployed
- Regenerate Twilio Auth Token — was exposed in a document during this session
- Consider adding a shared secret / API key to webhook before going to production (Phase 3 hardening task)
- Supervisor: confirm which test bus and when access is available