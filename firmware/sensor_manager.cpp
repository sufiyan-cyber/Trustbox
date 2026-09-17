#include "sensor_manager.h"

SensorManager sensors;

// Global instance pointer for interrupt
static SensorManager* globalSensorManager = nullptr;

void IRAM_ATTR SensorManager::handleMotionInterrupt() {
    if (globalSensorManager) {
        globalSensorManager->presenceDetected = true;
    }
}

SensorManager::SensorManager() :
    lastDisputePinState(HIGH),
    disputeButtonDebouncedState(HIGH),
    lastDisputeDebounceTime(0),
    disputeButtonLatched(false),
    lastVerifyPinState(HIGH),
    verifyButtonDebouncedState(HIGH),
    lastVerifyDebounceTime(0),
    verifyButtonLatched(false),
    presenceDetected(false),
    lastActivityTime(0),
    currentLedColor(LED_STATE_OFF)
{
    globalSensorManager = this;
}

void SensorManager::begin() {
    // Buttons configured as INPUT_PULLUP (Active LOW when pressed to GND)
    pinMode(PIN_BTN_DISPUTE, INPUT_PULLUP);
    pinMode(PIN_BTN_VERIFY, INPUT_PULLUP);

    // Motion Sensor configured as INPUT (PIR outputs HIGH on motion)
    pinMode(PIN_PIR_MOTION, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_PIR_MOTION), handleMotionInterrupt, RISING);

    // RGB LED Pins
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    setLedColor(LED_STATE_OFF);

    lastActivityTime = millis();
    Serial.println(F("[SENSORS] GPIO initialized: Dispute (Pin 13), Verify (Pin 12), PIR (Pin 27), RGB (14,32,33)."));
}

void SensorManager::registerActivity() {
    lastActivityTime = millis();
}

void SensorManager::update() {
    unsigned long currentMillis = millis();

    // 1. Debounce Dispute Button (PIN_BTN_DISPUTE)
    int disputeReading = digitalRead(PIN_BTN_DISPUTE);
    if (disputeReading != lastDisputePinState) {
        lastDisputeDebounceTime = currentMillis;
    }
    if ((currentMillis - lastDisputeDebounceTime) > BUTTON_DEBOUNCE_MS) {
        if (disputeReading != disputeButtonDebouncedState) {
            disputeButtonDebouncedState = disputeReading;
            if (disputeButtonDebouncedState == LOW) { // Pressed
                disputeButtonLatched = true;
                registerActivity();
                Serial.println(F("[HARDWARE] 🔺 Dispute button pressed!"));
            }
        }
    }
    lastDisputePinState = disputeReading;

    // 2. Debounce Verify Button (PIN_BTN_VERIFY)
    int verifyReading = digitalRead(PIN_BTN_VERIFY);
    if (verifyReading != lastVerifyPinState) {
        lastVerifyDebounceTime = currentMillis;
    }
    if ((currentMillis - lastVerifyDebounceTime) > BUTTON_DEBOUNCE_MS) {
        if (verifyReading != verifyButtonDebouncedState) {
            verifyButtonDebouncedState = verifyReading;
            if (verifyButtonDebouncedState == LOW) { // Pressed
                verifyButtonLatched = true;
                registerActivity();
                Serial.println(F("[HARDWARE] [Verify] button pressed!"));
            }
        }
    }
    lastVerifyPinState = verifyReading;

    // 3. Motion Sensor Polling Fallback
    if (digitalRead(PIN_PIR_MOTION) == HIGH) {
        presenceDetected = true;
        registerActivity();
    }
}

bool SensorManager::wasDisputeButtonPressed() {
    if (disputeButtonLatched) {
        disputeButtonLatched = false;
        return true;
    }
    return false;
}

bool SensorManager::wasVerifyButtonPressed() {
    if (verifyButtonLatched) {
        verifyButtonLatched = false;
        return true;
    }
    return false;
}

void SensorManager::setLedColor(LedColorState color) {
    currentLedColor = color;
    switch (color) {
        case LED_STATE_GREEN:
            digitalWrite(PIN_LED_RED, LOW);
            digitalWrite(PIN_LED_GREEN, HIGH);
            digitalWrite(PIN_LED_BLUE, LOW);
            break;
        case LED_STATE_YELLOW:
            digitalWrite(PIN_LED_RED, HIGH);
            digitalWrite(PIN_LED_GREEN, HIGH);
            digitalWrite(PIN_LED_BLUE, LOW);
            break;
        case LED_STATE_RED:
            digitalWrite(PIN_LED_RED, HIGH);
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_BLUE, LOW);
            break;
        case LED_STATE_BLUE:
            digitalWrite(PIN_LED_RED, LOW);
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_BLUE, HIGH);
            break;
        case LED_STATE_OFF:
        default:
            digitalWrite(PIN_LED_RED, LOW);
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_BLUE, LOW);
            break;
    }
}

void SensorManager::pulseLed(LedColorState color, int count, int intervalMs) {
    for (int i = 0; i < count; i++) {
        setLedColor(color);
        delay(intervalMs);
        setLedColor(LED_STATE_OFF);
        delay(intervalMs);
    }
    setLedColor(color);
}
