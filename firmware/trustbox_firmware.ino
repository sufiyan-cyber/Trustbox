/**
 * ============================================================================
 * PAYTM TRUSTBOX: FRAUD-PROOF UPI VERIFICATION DEVICE
 * ============================================================================
 * Firmware Target: ESP32 DevKit V1 (ESP-WROOM-32)
 * Pipeline: Sense -> Connect -> Investigate -> Predict -> Act
 * 
 * Hardware Modules:
 * - 3.2" SPI TFT LCD (ILI9341) & Touch (XPT2046)
 * - Motion / Presence IR Sensor (PIR HC-SR501 / FC-51)
 * - Pushbuttons: Dispute (🔺) & Verify
 * - RGB Status LED (Common Cathode)
 * - Speaker / Piezo Buzzer (PWM / I2S DAC)
 * - Microphone (Analog / INMP441 I2S)
 * ============================================================================
 */

#include <Arduino.h>
#include "config.h"
#include "display_ui.h"
#include "sensor_manager.h"
#include "audio_manager.h"
#include "network_client.h"

// Runtime State Variables
String currentMerchantId = DEFAULT_MERCHANT_ID;
String currentMerchantName = DEFAULT_MERCHANT_NAME;
String lastQueriedOrderId = "";
bool isDeviceAsleep = false;
unsigned long stateReturnTimer = 0;
bool pendingReturnToIdle = false;

// Forward Declarations
void handleDisputeAction();
void handleVerifyAction(double expectedAmount = 0.0);
void handleVoiceAction(const String& query);
void processSerialCli();
void printHelpMenu();

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);

    Serial.println();
    Serial.println(F("=================================================="));
    Serial.println(F("   PAYTM TRUSTBOX: FRAUD-PROOF UPI TERMINAL       "));
    Serial.printf( "   Firmware: %s | Device ID: %s\n", FIRMWARE_VERSION, TRUSTBOX_DEVICE_ID);
    Serial.println(F("=================================================="));

    // Initialize Subsystems
    display.begin();
    sensors.begin();
    audio.begin();

    // Boot Test Sequence
    sensors.pulseLed(LED_STATE_BLUE, 2, 100);
    audio.playSound(SOUND_WAKE_UP);

    // Initialize Network (Non-blocking if offline)
    network.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);

    // Initial Display State
    display.showHomeScreen(currentMerchantName.c_str(), currentMerchantId.c_str());
    sensors.setLedColor(LED_STATE_OFF);

    printHelpMenu();
}

void loop() {
    // 1. Update Sensors & Button Debounce
    sensors.update();
    display.tick();

    // 2. Handle Wake-on-Presence (IR Motion)
    if (sensors.isPresenceDetected()) {
        if (isDeviceAsleep) {
            isDeviceAsleep = false;
            display.setBacklight(true);
            audio.playSound(SOUND_WAKE_UP);
            display.showHomeScreen(currentMerchantName.c_str(), currentMerchantId.c_str());
            Serial.println(F("[TRUSTBOX] Woke up from presence detection."));
        }
    }

    // 3. Auto-Sleep Timeout check
    if (!isDeviceAsleep && (millis() - sensors.getLastActivityTime() > SLEEP_TIMEOUT_MS)) {
        isDeviceAsleep = true;
        display.setBacklight(false);
        sensors.setLedColor(LED_STATE_OFF);
        Serial.println(F("[TRUSTBOX] Inactivity timeout: Screen dimmed to sleep mode."));
    }

    // 4. Sense: Check Physical Pushbuttons
    if (sensors.wasDisputeButtonPressed()) {
        audio.playSound(SOUND_BUTTON_CLICK);
        handleDisputeAction();
    } else if (sensors.wasVerifyButtonPressed()) {
        audio.playSound(SOUND_BUTTON_CLICK);
        handleVerifyAction();
    }

    // 5. Sense: Check Acoustic Mic Voice Trigger
    if (audio.isVoiceTriggerDetected()) {
        Serial.println(F("[TRUSTBOX] Voice trigger threshold reached: 'Kitna aaya?'"));
        handleVoiceAction("Kitna aaya?");
    }

    // 6. Handle Auto-Return to Idle Screen after displaying result
    if (pendingReturnToIdle && millis() > stateReturnTimer) {
        pendingReturnToIdle = false;
        display.showHomeScreen(currentMerchantName.c_str(), currentMerchantId.c_str());
        sensors.setLedColor(LED_STATE_OFF);
    }

    // 7. Handle Serial Diagnostic CLI Commands
    processSerialCli();

    delay(10);
}

// ----------------------------------------------------------------------------
// PIPELINE: VERIFICATION (Sense -> Connect -> Investigate -> Predict -> Act)
// ----------------------------------------------------------------------------
void handleVerifyAction(double expectedAmount) {
    Serial.println(F("\n[PIPELINE] >> Step 1: Sense - Verify Triggered"));
    sensors.setLedColor(LED_STATE_YELLOW);
    display.showVerifyingScreen("Check latest payment");

    Serial.println(F("[PIPELINE] >> Step 2: Connect - Sending query to Backend"));
    VerifyResult res = network.verifyPayment(currentMerchantId.c_str(), lastQueriedOrderId.c_str(), expectedAmount);

    Serial.println(F("[PIPELINE] >> Step 3 & 4: Investigate & Predict - Response received"));
    Serial.printf("[PIPELINE] Status: %s | Amount: %.2f | FraudScore: %.2f | Level: %s\n",
                  res.status.c_str(), res.amount, res.fraudScore, res.alertLevel.c_str());

    Serial.println(F("[PIPELINE] >> Step 5: Act - Executing Voice & Visual Response"));

    if (!res.success && res.rawError.length() > 0) {
        // Network Error Fallback
        sensors.setLedColor(LED_STATE_RED);
        display.showNetworkErrorScreen(res.rawError.c_str());
        audio.speakText("Unable to connect to server. Please check your network.", "en");
    } else if (res.fraudScore >= 0.70f) {
        // High Risk Fraud Detected
        sensors.setLedColor(LED_STATE_RED);
        display.showFraudAlertScreen(res.amount, res.fraudScore, res.message.c_str());
        audio.playSound(SOUND_FRAUD_ALERT);
        audio.speakText(res.ttsText.length() > 0 ? res.ttsText.c_str() : "Warning: Suspicious transaction detected. Press dispute button.", "hi");
    } else if (res.status == "not_found") {
        // No payment found
        sensors.setLedColor(LED_STATE_RED);
        display.showNotFoundScreen(res.message.c_str());
        audio.playSound(SOUND_BUTTON_CLICK);
        audio.speakText(res.ttsText.length() > 0 ? res.ttsText.c_str() : "Koi payment nahi mila.", "hi");
    } else {
        // Genuine Payment Confirmed
        sensors.setLedColor(LED_STATE_GREEN);
        display.showSuccessScreen(res.amount, "Just Now", res.fraudScore);
        audio.playSound(SOUND_PAYMENT_SUCCESS);
        char confirmSpeech[128];
        snprintf(confirmSpeech, sizeof(confirmSpeech), "Paytm TrustBox: %.0f rupaye prapt hue.", res.amount);
        audio.speakText(confirmSpeech, "hi");
    }

    // Schedule return to idle after 8 seconds
    stateReturnTimer = millis() + 8000;
    pendingReturnToIdle = true;
}

// ----------------------------------------------------------------------------
// PIPELINE: VOICE QUERY ("Kitna aaya?")
// ----------------------------------------------------------------------------
void handleVoiceAction(const String& query) {
    Serial.printf("\n[VOICE] Query Received: \"%s\"\n", query.c_str());
    display.showVerifyingScreen(query.c_str());
    sensors.setLedColor(LED_STATE_YELLOW);

    VerifyResult res = network.sendVoiceQuery(currentMerchantId.c_str(), query.c_str());

    if (res.status == "confirmed") {
        sensors.setLedColor(LED_STATE_GREEN);
        display.showSuccessScreen(res.amount, "Just Now", res.fraudScore);
        audio.playSound(SOUND_PAYMENT_SUCCESS);
        audio.speakText(res.ttsText.c_str(), "hi");
    } else if (res.fraudScore >= 0.70f) {
        sensors.setLedColor(LED_STATE_RED);
        display.showFraudAlertScreen(res.amount, res.fraudScore, res.message.c_str());
        audio.playSound(SOUND_FRAUD_ALERT);
        audio.speakText(res.ttsText.c_str(), "hi");
    } else {
        sensors.setLedColor(LED_STATE_RED);
        display.showNotFoundScreen(res.message.c_str());
        audio.speakText("Koi payment nahi mila.", "hi");
    }

    stateReturnTimer = millis() + 8000;
    pendingReturnToIdle = true;
}

// ----------------------------------------------------------------------------
// PIPELINE: DISPUTE ESCALATION (🔺 Button)
// ----------------------------------------------------------------------------
void handleDisputeAction() {
    Serial.println(F("\n[DISPUTE] >> Merchant pressed 🔺 Dispute button!"));
    sensors.setLedColor(LED_STATE_YELLOW);
    display.showVerifyingScreen("Escalating dispute...");

    VerifyResult res = network.reportDispute(currentMerchantId.c_str(), lastQueriedOrderId.c_str(), "Merchant reported suspicious transaction");

    sensors.setLedColor(LED_STATE_RED);
    display.showDisputeRaisedScreen(res.ticketId.c_str());
    audio.playSound(SOUND_DISPUTE_RAISED);
    audio.speakText("Dispute registered. Paytm support is contacting you.", "en");

    stateReturnTimer = millis() + 10000;
    pendingReturnToIdle = true;
}

// ----------------------------------------------------------------------------
// SERIAL CLI DIAGNOSTIC CONSOLE
// ----------------------------------------------------------------------------
void printHelpMenu() {
    Serial.println(F("\n--- Paytm TrustBox Serial Diagnostic CLI ---"));
    Serial.println(F("Commands available:"));
    Serial.println(F("  STATUS           : Print hardware status, Wi-Fi, and pins"));
    Serial.println(F("  WAKE             : Trigger motion presence wake-up"));
    Serial.println(F("  VERIFY [amt]     : Trigger verification (e.g. 'VERIFY 500')"));
    Serial.println(F("  DISPUTE          : Trigger 🔺 Dispute button event"));
    Serial.println(F("  VOICE <query>    : Trigger voice query (e.g. 'VOICE Kitna aaya?')"));
    Serial.println(F("  TEST_LED         : Test RGB LED (Red, Yellow, Green, Blue)"));
    Serial.println(F("  TEST_AUDIO       : Test success chime and fraud alert siren"));
    Serial.println(F("  TEST_DISPLAY     : Cycle through TFT UI screens"));
    Serial.println(F("  SET_SERVER <url> : Update backend URL (e.g. 'SET_SERVER http://192.168.1.100:3000')"));
    Serial.println(F("  SET_MID <mid>    : Update Merchant ID (e.g. 'SET_MID M98765432')"));
    Serial.println(F("--------------------------------------------\n"));
}

void processSerialCli() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    Serial.printf("[CLI] > %s\n", cmd.c_str());

    if (cmd.equalsIgnoreCase("HELP")) {
        printHelpMenu();
    } else if (cmd.equalsIgnoreCase("STATUS")) {
        Serial.printf("Device ID: %s | Version: %s\n", TRUSTBOX_DEVICE_ID, FIRMWARE_VERSION);
        Serial.printf("Merchant: %s (%s)\n", currentMerchantName.c_str(), currentMerchantId.c_str());
        Serial.printf("Wi-Fi Status: %s | IP: %s\n", network.isConnected() ? "CONNECTED" : "OFFLINE", network.getIpAddress().c_str());
        Serial.printf("Backend URL: %s\n", network.getServerUrl().c_str());
        Serial.printf("Dispute Pin (13): %s | Verify Pin (12): %s | PIR Pin (27): %d\n",
                      digitalRead(PIN_BTN_DISPUTE) == LOW ? "PRESSED" : "IDLE",
                      digitalRead(PIN_BTN_VERIFY) == LOW ? "PRESSED" : "IDLE",
                      digitalRead(PIN_PIR_MOTION));
    } else if (cmd.equalsIgnoreCase("WAKE")) {
        isDeviceAsleep = false;
        display.setBacklight(true);
        audio.playSound(SOUND_WAKE_UP);
        display.showHomeScreen(currentMerchantName.c_str(), currentMerchantId.c_str());
        Serial.println(F("[CLI] Device awakened."));
    } else if (cmd.startsWith("VERIFY") || cmd.startsWith("verify")) {
        double amt = 0.0;
        int spaceIdx = cmd.indexOf(' ');
        if (spaceIdx > 0) {
            amt = cmd.substring(spaceIdx + 1).toDouble();
        }
        handleVerifyAction(amt);
    } else if (cmd.equalsIgnoreCase("DISPUTE")) {
        handleDisputeAction();
    } else if (cmd.startsWith("VOICE ") || cmd.startsWith("voice ")) {
        String query = cmd.substring(6);
        handleVoiceAction(query);
    } else if (cmd.equalsIgnoreCase("TEST_LED")) {
        Serial.println(F("[CLI] Testing RGB LED sequence..."));
        sensors.setLedColor(LED_STATE_RED);
        delay(500);
        sensors.setLedColor(LED_STATE_YELLOW);
        delay(500);
        sensors.setLedColor(LED_STATE_GREEN);
        delay(500);
        sensors.setLedColor(LED_STATE_BLUE);
        delay(500);
        sensors.setLedColor(LED_STATE_OFF);
        Serial.println(F("[CLI] RGB LED test complete."));
    } else if (cmd.equalsIgnoreCase("TEST_AUDIO")) {
        Serial.println(F("[CLI] Testing Audio output..."));
        audio.playSound(SOUND_PAYMENT_SUCCESS);
        delay(500);
        audio.playSound(SOUND_FRAUD_ALERT);
        delay(500);
        audio.speakText("Paytm TrustBox audio test complete.", "en");
    } else if (cmd.equalsIgnoreCase("TEST_DISPLAY")) {
        Serial.println(F("[CLI] Testing TFT screens..."));
        display.showHomeScreen(currentMerchantName.c_str(), currentMerchantId.c_str());
        delay(1200);
        display.showVerifyingScreen("Test Payment ORD-100");
        delay(1200);
        display.showSuccessScreen(500.0, "12:30 PM", 0.05f);
        delay(1200);
        display.showFraudAlertScreen(5000.0, 0.95f, "Fake Screenshot / Missing Record");
        delay(1200);
        display.showDisputeRaisedScreen("TKT-8831");
        delay(1200);
        display.showHomeScreen(currentMerchantName.c_str(), currentMerchantId.c_str());
        Serial.println(F("[CLI] Display test complete."));
    } else if (cmd.startsWith("SET_SERVER ")) {
        String newUrl = cmd.substring(11);
        newUrl.trim();
        network.setServerUrl(newUrl);
        Serial.printf("[CLI] Backend URL updated to: %s\n", newUrl.c_str());
    } else if (cmd.startsWith("SET_MID ")) {
        String newMid = cmd.substring(8);
        newMid.trim();
        currentMerchantId = newMid;
        Serial.printf("[CLI] Merchant ID updated to: %s\n", currentMerchantId.c_str());
        display.showHomeScreen(currentMerchantName.c_str(), currentMerchantId.c_str());
    } else {
        Serial.println(F("[CLI] Unknown command. Type 'HELP' for list of commands."));
    }
}
