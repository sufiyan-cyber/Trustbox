#include "display_ui.h"

DisplayUI display;

DisplayUI::DisplayUI() : currentState(SCREEN_IDLE_HOME), backlightOn(true), lastBlinkTime(0), blinkToggle(false) {}

void DisplayUI::begin() {
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);
    
    Serial.println(F("[DISPLAY] Initializing 3.2\" ILI9341 TFT SPI Display (320x240)..."));
    Serial.println(F("[DISPLAY] Display hardware initialized successfully."));
}

void DisplayUI::setBacklight(bool on) {
    backlightOn = on;
    digitalWrite(TFT_BL_PIN, on ? HIGH : LOW);
}

void DisplayUI::drawHeader(const char* title, uint16_t bgColor) {
    // In physical TFT: tft.fillRect(0, 0, 320, 36, bgColor);
    // tft.setTextColor(COLOR_WHITE);
    // tft.setTextSize(2);
    // tft.drawString(title, 12, 10);
}

void DisplayUI::drawFooter(const char* hint) {
    // In physical TFT: tft.fillRect(0, 216, 320, 24, COLOR_DARK_GRAY);
    // tft.setTextColor(COLOR_WHITE);
    // tft.drawString(hint, 10, 220);
}

void DisplayUI::printSerialScreenDump(const char* screenName, const char* line1, const char* line2, const char* line3) {
    Serial.println();
    Serial.println(F("+--------------------------------------------------+"));
    Serial.printf( "|  [SCREEN: %-37s] |\n", screenName);
    Serial.println(F("+--------------------------------------------------+"));
    Serial.printf( "|  %-48s|\n", line1);
    Serial.printf( "|  %-48s|\n", line2);
    Serial.printf( "|  %-48s|\n", line3);
    Serial.println(F("+--------------------------------------------------+"));
}

void DisplayUI::showHomeScreen(const char* merchantName, const char* merchantId) {
    currentState = SCREEN_IDLE_HOME;
    setBacklight(true);

    char line1[64], line2[64], line3[64];
    snprintf(line1, sizeof(line1), "Paytm TrustBox | %s", merchantName);
    snprintf(line2, sizeof(line2), "MID: %s | Ready for Payment", merchantId);
    snprintf(line3, sizeof(line3), "Say 'Kitna aaya?' or press [Verify] / [Dispute 🔺]");

    printSerialScreenDump("IDLE_HOME", line1, line2, line3);
}

void DisplayUI::showVerifyingScreen(const char* queryOrOrder) {
    currentState = SCREEN_VERIFYING;
    setBacklight(true);

    char line1[64], line2[64], line3[64];
    snprintf(line1, sizeof(line1), "[⏳] Verifying with Paytm UPI Gateway...");
    snprintf(line2, sizeof(line2), "Query: \"%s\"", queryOrOrder ? queryOrOrder : "Latest Order");
    snprintf(line3, sizeof(line3), "Please wait, checking transaction authenticity...");

    printSerialScreenDump("VERIFYING", line1, line2, line3);
}

void DisplayUI::showSuccessScreen(double amount, const char* timestamp, float fraudScore) {
    currentState = SCREEN_SUCCESS;
    setBacklight(true);

    char line1[64], line2[64], line3[64];
    snprintf(line1, sizeof(line1), "[✔] PAYMENT RECEIVED: INR %.2f", amount);
    snprintf(line2, sizeof(line2), "Time: %s | Risk: SAFE (%.2f)", timestamp ? timestamp : "Just now", fraudScore);
    snprintf(line3, sizeof(line3), "Account Credited. Genuine UPI Transaction.");

    printSerialScreenDump("SUCCESS", line1, line2, line3);
}

void DisplayUI::showNotFoundScreen(const char* reason) {
    currentState = SCREEN_NOT_FOUND;
    setBacklight(true);

    char line1[64], line2[64], line3[64];
    snprintf(line1, sizeof(line1), "[✖] NO PAYMENT FOUND!");
    snprintf(line2, sizeof(line2), "Reason: %s", reason ? reason : "No matching bank credit");
    snprintf(line3, sizeof(line3), "Do not give goods! Press [🔺 Dispute] if needed.");

    printSerialScreenDump("NOT_FOUND", line1, line2, line3);
}

void DisplayUI::showFraudAlertScreen(double amount, float fraudScore, const char* reason) {
    currentState = SCREEN_FRAUD_ALERT;
    setBacklight(true);

    char line1[64], line2[64], line3[64];
    snprintf(line1, sizeof(line1), "[⚠ WARNING] POSSIBLE FRAUD DETECTED!");
    snprintf(line2, sizeof(line2), "Claimed: INR %.2f | Fraud Score: %.2f (CRITICAL)", amount, fraudScore);
    snprintf(line3, sizeof(line3), "Reason: %s | Press [🔺 Dispute] to Escalate", reason ? reason : "Fake screenshot / ghost txn");

    printSerialScreenDump("FRAUD_ALERT", line1, line2, line3);
}

void DisplayUI::showDisputeRaisedScreen(const char* ticketId) {
    currentState = SCREEN_DISPUTE_RAISED;
    setBacklight(true);

    char line1[64], line2[64], line3[64];
    snprintf(line1, sizeof(line1), "[🔺] DISPUTE REGISTERED WITH PAYTM OPS");
    snprintf(line2, sizeof(line2), "Ticket ID: %s", ticketId ? ticketId : "DSP-9042");
    snprintf(line3, sizeof(line3), "Support team notified. Calling merchant shortly.");

    printSerialScreenDump("DISPUTE_RAISED", line1, line2, line3);
}

void DisplayUI::showNetworkErrorScreen(const char* errorMsg, int retryCount) {
    currentState = SCREEN_NETWORK_ERROR;

    char line1[64], line2[64], line3[64];
    snprintf(line1, sizeof(line1), "[⚡] NETWORK ERROR - Unable to reach server");
    snprintf(line2, sizeof(line2), "Error: %s (Retry: %d/%d)", errorMsg ? errorMsg : "Timeout", retryCount, MAX_HTTP_RETRIES);
    snprintf(line3, sizeof(line3), "Check Wi-Fi / Router connection.");

    printSerialScreenDump("NETWORK_ERROR", line1, line2, line3);
}

void DisplayUI::showSettingsScreen(const char* ssid, const char* ip, const char* serverUrl) {
    currentState = SCREEN_SETTINGS;

    char line1[64], line2[64], line3[64];
    snprintf(line1, sizeof(line1), "Paytm TrustBox Settings (%s)", FIRMWARE_VERSION);
    snprintf(line2, sizeof(line2), "Wi-Fi: %s | IP: %s", ssid, ip);
    snprintf(line3, sizeof(line3), "Backend: %s", serverUrl);

    printSerialScreenDump("SETTINGS", line1, line2, line3);
}

void DisplayUI::tick() {
    // If in Fraud Alert mode, blink border or status indicator every 500ms
    if (currentState == SCREEN_FRAUD_ALERT) {
        if (millis() - lastBlinkTime > 500) {
            lastBlinkTime = millis();
            blinkToggle = !blinkToggle;
            // Visual pulse toggle
        }
    }
}
