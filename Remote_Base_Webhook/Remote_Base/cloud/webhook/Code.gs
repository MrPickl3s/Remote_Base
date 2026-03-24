// ============================================================
// Remote Base — Webhook Receiver
// Google Apps Script deployed as a web app
// Receives JSON POST from Freematics dongle, sends SMS via Twilio
// ============================================================

// --- CONFIGURATION ---
// Replace these with your actual values
var CONFIG = {
  TWILIO_ACCOUNT_SID: "YOUR_TWILIO_ACCOUNT_SID",
  TWILIO_AUTH_TOKEN: "YOUR_TWILIO_AUTH_TOKEN",
  TWILIO_FROM_NUMBER: "+1XXXXXXXXXX",  // Your Twilio trial number
  MECHANIC_PHONE: "+1XXXXXXXXXX",      // Mechanic's phone (must be verified on Twilio trial)
};

// --- MAIN ENTRY POINT ---
// Google Apps Script calls this when something POSTs to the deployed URL
function doPost(e) {
  try {
    var payload = JSON.parse(e.postData.contents);
    
    // Validate required fields
    if (!payload.bus_id || !payload.event_type || !payload.timestamp) {
      return buildResponse(400, { error: "Missing required fields: bus_id, event_type, timestamp" });
    }
    
    // Route by event type
    if (payload.event_type === "fault") {
      handleFault(payload);
    } else if (payload.event_type === "heartbeat") {
      handleHeartbeat(payload);
    } else if (payload.event_type === "checkin") {
      handleCheckin(payload);
    } else if (payload.event_type === "threshold_alert") {
      handleThresholdAlert(payload);
    } else {
      return buildResponse(400, { error: "Unknown event_type: " + payload.event_type });
    }
    
    return buildResponse(200, { status: "ok", event_type: payload.event_type });
    
  } catch (err) {
    Logger.log("doPost error: " + err.toString());
    return buildResponse(500, { error: err.toString() });
  }
}

// --- EVENT HANDLERS ---

function handleFault(payload) {
  if (!payload.faults || payload.faults.length === 0) {
    Logger.log("Fault event with no faults array for " + payload.bus_id);
    return;
  }
  
  var smsBody = formatFaultSMS(payload);
  sendSMS(CONFIG.MECHANIC_PHONE, smsBody);
  logEvent(payload);
}

function handleHeartbeat(payload) {
  // Phase 3+ — just log it for now
  logEvent(payload);
}

function handleCheckin(payload) {
  // Phase 3+ — log daily mileage/hours
  logEvent(payload);
}

function handleThresholdAlert(payload) {
  // Phase 3+ — will send SMS for threshold breaches
  logEvent(payload);
}

// --- SMS FORMATTING ---

function formatFaultSMS(payload) {
  var lines = [];
  
  // Header with buffered indicator
  var header = "FAULT — " + formatBusId(payload.bus_id);
  if (payload.meta && payload.meta.buffered) {
    header += " [DELAYED]";
  }
  lines.push(header);
  
  // Each fault on its own line
  for (var i = 0; i < payload.faults.length; i++) {
    var f = payload.faults[i];
    var faultLine = "SPN " + f.spn + " / FMI " + f.fmi;
    if (f.occurrence && f.occurrence > 0) {
      faultLine += " (x" + f.occurrence + ")";
    }
    lines.push(faultLine);
  }
  
  // Timestamp — convert ISO to readable local time
  lines.push(formatTimestamp(payload.timestamp));
  
  return lines.join("\n");
}

// --- HELPERS ---

function formatBusId(busId) {
  // "bus_07" -> "Bus 07"
  return busId.replace("bus_", "Bus ");
}

function formatTimestamp(isoString) {
  try {
    var dt = new Date(isoString);
    // Format as "Mar 23, 10:45 AM" in the script's timezone
    return Utilities.formatDate(dt, Session.getScriptTimeZone(), "MMM d, h:mm a");
  } catch (e) {
    return isoString;
  }
}

function buildResponse(statusCode, body) {
  return ContentService
    .createTextOutput(JSON.stringify(body))
    .setMimeType(ContentService.MimeType.JSON);
}

// --- TWILIO SMS ---

function sendSMS(toNumber, body) {
  var url = "https://api.twilio.com/2010-04-01/Accounts/" 
            + CONFIG.TWILIO_ACCOUNT_SID 
            + "/Messages.json";
  
  var options = {
    method: "post",
    headers: {
      "Authorization": "Basic " + Utilities.base64Encode(
        CONFIG.TWILIO_ACCOUNT_SID + ":" + CONFIG.TWILIO_AUTH_TOKEN
      )
    },
    payload: {
      "To": toNumber,
      "From": CONFIG.TWILIO_FROM_NUMBER,
      "Body": body
    },
    muteHttpExceptions: true
  };
  
  var response = UrlFetchApp.fetch(url, options);
  var responseCode = response.getResponseCode();
  
  if (responseCode !== 201) {
    Logger.log("Twilio error (" + responseCode + "): " + response.getContentText());
    throw new Error("Twilio SMS failed with status " + responseCode);
  }
  
  Logger.log("SMS sent to " + toNumber + ": " + body);
}

// --- EVENT LOGGING ---
// Logs to Apps Script's built-in Logger for now
// Phase 2+: will log to Google Sheet or Supabase

function logEvent(payload) {
  Logger.log("Event received: " + JSON.stringify(payload));
}

// --- TEST FUNCTION ---
// Run this manually in the Apps Script editor to test SMS without deploying
function testSendFault() {
  var mockPayload = {
    bus_id: "bus_07",
    event_type: "fault",
    timestamp: new Date().toISOString(),
    faults: [
      { spn: 3226, fmi: 0, occurrence: 3 },
      { spn: 110, fmi: 16, occurrence: 1 }
    ],
    meta: {
      firmware_version: "0.1.0",
      signal_strength: -85,
      battery_voltage: 24.1,
      buffered: false
    }
  };
  
  var smsBody = formatFaultSMS(mockPayload);
  Logger.log("SMS preview:\n" + smsBody);
  
  // Uncomment the next line to actually send the SMS:
  // sendSMS(CONFIG.MECHANIC_PHONE, smsBody);
}
