#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>
#include "config.h"

// UI Screen States
enum DisplayScreenState {
    SCREEN_IDLE_HOME,
    SCREEN_VERIFYING,
    SCREEN_SUCCESS,
    SCREEN_NOT_FOUND,
    SCREEN_FRAUD_ALERT,
    SCREEN_DISPUTE_RAISED,
    SCREEN_NETWORK_ERROR,
    SCREEN_SETTINGS
};

// UI Color Palette (RGB565 format for TFT displays)
#define COLOR_NAVY_BLUE   0x0113   // Paytm Signature Blue
#define COLOR_CYAN_BLUE   0x05BF   // Paytm Accent Cyan
#define COLOR_WHITE       0xFFFF
#define COLOR_BLACK       0x0000
#define COLOR_GRAY        0x7BEF
#define COLOR_DARK_GRAY   0x39E7
#define COLOR_SUCCESS_GRN 0x05E0   // Vivid Green
#define COLOR_ALERT_RED   0xF800   // Warning / Alert Red
#define COLOR_WARN_YELLOW 0xFFE0   // Pending Yellow

class DisplayUI {
public:
    DisplayUI();
    void begin();
    
    // Screen Render Methods
    void showHomeScreen(const char* merchantName, const char* merchantId);
    void showVerifyingScreen(const char* queryOrOrder);
    void showSuccessScreen(double amount, const char* timestamp, float fraudScore);
    void showNotFoundScreen(const char* reason = "No payment found");
    void showFraudAlertScreen(double amount, float fraudScore, const char* reason);
    void showDisputeRaisedScreen(const char* ticketId);
    void showNetworkErrorScreen(const char* errorMsg, int retryCount = 0);
    void showSettingsScreen(const char* ssid, const char* ip, const char* serverUrl);

    // Screen Management
    DisplayScreenState getCurrentState() const { return currentState; }
    void setBacklight(bool on);
    void tick(); // Handles animations/border blinking if needed

private:
    DisplayScreenState currentState;
    bool backlightOn;
    unsigned long lastBlinkTime;
    bool blinkToggle;

    void drawHeader(const char* title, uint16_t bgColor = COLOR_NAVY_BLUE);
    void drawFooter(const char* hint);
    void printSerialScreenDump(const char* screenName, const char* line1, const char* line2, const char* line3);
};

extern DisplayUI display;

#endif // DISPLAY_UI_H
