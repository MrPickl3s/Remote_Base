# Webhook Deployment Guide

## What This Is
A Google Apps Script that receives JSON POST requests from the Freematics dongle and sends SMS alerts to the mechanic via Twilio. This guide walks you through deploying it and testing it.

---

## Step 1: Create the Apps Script Project

1. Go to **script.google.com**
2. Click **New project**
3. It opens with a file called `Code.gs` containing an empty `myFunction()`
4. **Delete everything** in that file
5. Open `cloud/webhook/Code.gs` from your repo, **copy the entire contents**
6. Paste it into the Apps Script editor, replacing everything

## Step 2: Add Your Twilio Credentials

At the top of `Code.gs`, replace the placeholder values in the `CONFIG` object:

```javascript
var CONFIG = {
  TWILIO_ACCOUNT_SID: "ACxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",  // From Twilio dashboard
  TWILIO_AUTH_TOKEN: "your_auth_token_here",                   // From Twilio dashboard
  TWILIO_FROM_NUMBER: "+15551234567",                          // Your Twilio trial number
  MECHANIC_PHONE: "+15559876543",                              // Your phone (verified in Twilio)
};
```

**Where to find these:**
- Log into **twilio.com/console**
- Account SID and Auth Token are right on the dashboard
- Your Twilio phone number is under **Phone Numbers > Manage > Active Numbers**

**Important:** On the Twilio free trial, you can only send SMS to phone numbers you've verified. Your own number was verified at signup. To add the mechanic's number later, go to **Phone Numbers > Verified Caller IDs**.

## Step 3: Test SMS Without Deploying

Before deploying the web app, test that Twilio works:

1. In the Apps Script editor, find the `testSendFault()` function at the bottom
2. **Uncomment** the `sendSMS()` line (remove the `//`)
3. Select `testSendFault` from the function dropdown at the top
4. Click **Run**
5. Google will ask you to authorize the script — click through the permissions
6. Check your phone — you should get an SMS that looks like:

```
FAULT — Bus 07
SPN 3226 / FMI 0 (x3)
SPN 110 / FMI 16 (x1)
Mar 23, 10:45 AM
```

If it works, **re-comment the sendSMS line** so you don't accidentally send SMS every time you test formatting.

**Troubleshooting:**
- Check **View > Executions** in the Apps Script editor for error logs
- "Twilio SMS failed with status 401" = wrong Account SID or Auth Token
- "Twilio SMS failed with status 400" = wrong phone number format (must be +1XXXXXXXXXX)

## Step 4: Deploy as a Web App

This makes the script accessible via a URL that the dongle will POST to.

1. In the Apps Script editor, click **Deploy > New deployment**
2. Click the gear icon next to "Select type" and choose **Web app**
3. Set these options:
   - **Description:** "Remote Base webhook v1"
   - **Execute as:** Me
   - **Who has access:** Anyone
4. Click **Deploy**
5. Google will ask to authorize again — click through
6. You'll see a **Web app URL** — it looks like:
   `https://script.google.com/macros/s/AKfycb.../exec`
7. **Copy and save this URL.** This is what the dongle will POST to.

**"Anyone" access note:** This means anyone with the URL can POST to it. The URL itself is unguessable (long random string), so this is acceptable for Phase 1-3. Phase 5+ will add authentication.

## Step 5: Test with curl

Open your terminal and run this command (replace YOUR_WEBHOOK_URL):

```bash
curl -X POST "YOUR_WEBHOOK_URL" \
  -H "Content-Type: application/json" \
  -d '{
    "bus_id": "bus_07",
    "event_type": "fault",
    "timestamp": "2026-03-23T18:30:00Z",
    "faults": [
      { "spn": 3226, "fmi": 0, "occurrence": 3 }
    ],
    "meta": {
      "firmware_version": "0.1.0",
      "signal_strength": -85,
      "battery_voltage": 24.1
    }
  }'
```

**Expected result:**
- Terminal shows: `{"status":"ok","event_type":"fault"}`
- Your phone gets an SMS: `FAULT — Bus 07 / SPN 3226 / FMI 0 (x3) / Mar 23, 11:30 AM`

If this works, your cloud pipeline is done. The firmware just needs to POST to this URL.

## Step 6: Save the Webhook URL

Add the webhook URL to your repo for reference (but NOT the Twilio credentials):

```bash
cd Remote_Base
echo "WEBHOOK_URL=https://script.google.com/macros/s/YOUR_ID/exec" > cloud/webhook/.env.example
git add cloud/webhook/.env.example
git commit -m "Add webhook URL reference"
git push
```

---

## Updating the Script

If you need to change the code after deploying:

1. Edit in the Apps Script editor
2. Click **Deploy > Manage deployments**
3. Click the pencil icon on your deployment
4. Change **Version** to "New version"
5. Click **Deploy**
6. The URL stays the same — no firmware update needed

---

## What Comes Next

- **Phase 3:** Add config lookup from Google Sheet (per-bus mechanic phone, thresholds)
- **Phase 3:** Add threshold_alert SMS formatting
- **Phase 5:** Migrate to Supabase Edge Functions with proper auth
