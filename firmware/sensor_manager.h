#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "config.h"

enum LedColorState {
    LED_STATE_OFF,
    LED_STATE_GREEN,
    LED_STATE_YELLOW,
    LED_STATE_RED,
    LED_STATE_BLUE
};

class SensorManager {
public:
    SensorManager();
    void begin();
    void update();

    // Button Events (Latched, cleared on read)
    bool wasDisputeButtonPressed();
    bool wasVerifyButtonPressed();

    // Presence / Motion
    bool isPresenceDetected() const { return presenceDetected; }
    unsigned long getLastActivityTime() const { return lastActivityTime; }
    void registerActivity();

    // RGB LED Indicator
    void setLedColor(LedColorState color);
    void pulseLed(LedColorState color, int count = 3, int intervalMs = 200);

private:
    // Dispute Button Debounce
    int lastDisputePinState;
    int disputeButtonDebouncedState;
    unsigned long lastDisputeDebounceTime;
    bool disputeButtonLatched;

    // Verify Button Debounce
    int lastVerifyPinState;
    int verifyButtonDebouncedState;
    unsigned long lastVerifyDebounceTime;
    bool verifyButtonLatched;

    // Motion Sensor
    volatile bool presenceDetected;
    unsigned long lastActivityTime;
    LedColorState currentLedColor;

    static void IRAM_ATTR handleMotionInterrupt();
};

extern SensorManager sensors;

#endif // SENSOR_MANAGER_H
