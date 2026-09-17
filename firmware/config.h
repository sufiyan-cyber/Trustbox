#ifndef TRUSTBOX_CONFIG_H
#define TRUSTBOX_CONFIG_H

#include <Arduino.h>

// ==========================================
// PAYTM TRUSTBOX - FIRMWARE CONFIGURATION
// ==========================================

// Merchant & Device Identity
#define TRUSTBOX_DEVICE_ID      "TBX-IND-001"
#define DEFAULT_MERCHANT_ID     "M12345678"
#define DEFAULT_MERCHANT_NAME   "Rajesh Kirana Store"
#define FIRMWARE_VERSION        "v1.0.0-PROD"

// Network & Wi-Fi Settings
#define DEFAULT_WIFI_SSID       "Paytm_TrustBox_WiFi"
#define DEFAULT_WIFI_PASS       "trustbox2026"

// TrustBox Backend Server API Configuration
// Change this to your backend host IP or domain: e.g. "http://192.168.1.100:3000"
#define DEFAULT_SERVER_URL      "http://192.168.1.50:3000"
#define API_VERIFY_ENDPOINT     "/api/verify_payment"
#define API_DISPUTE_ENDPOINT    "/api/report_dispute"
#define API_VOICE_ENDPOINT      "/api/voice_query"
#define API_HEARTBEAT_ENDPOINT  "/api/heartbeat"

// Network Retry & Backoff Configuration
#define HTTP_TIMEOUT_MS         5000
#define MAX_HTTP_RETRIES        3
const unsigned long RETRY_DELAYS_MS[MAX_HTTP_RETRIES] = { 500, 1000, 2000 };

// Display Pin Configuration (3.2" SPI ILI9341 LCD)
#define TFT_CS_PIN              15
#define TFT_DC_PIN              2
#define TFT_RST_PIN             4
#define TFT_MOSI_PIN            23
#define TFT_CLK_PIN             18
#define TFT_MISO_PIN            19
#define TFT_BL_PIN              21   // Backlight PWM pin
#define TOUCH_CS_PIN            5

#define SCREEN_WIDTH            320
#define SCREEN_HEIGHT           240

// Pushbutton Pins (Active LOW with internal pull-up)
#define PIN_BTN_DISPUTE         13   // Red / Triangle (🔺) button
#define PIN_BTN_VERIFY          12   // Green / Quick-Verify button
#define BUTTON_DEBOUNCE_MS      50

// Motion & Presence Sensor
#define PIN_PIR_MOTION          27   // Wake-on-presence input

// Status RGB LED Pins (Common Cathode)
#define PIN_LED_RED             14
#define PIN_LED_GREEN           32
#define PIN_LED_BLUE            33

// Audio Output & Buzzer
#define PIN_AUDIO_DAC_OR_BUZZER 25   // PWM buzzer or I2S DIN
#define PIN_I2S_LRCK            26   // I2S Left/Right Clock (LRC)
#define PIN_I2S_BCLK            22   // I2S Bit Clock (BCLK)

// Microphone Pin (Analog or I2S SD)
#define PIN_MIC_INPUT           34

// Serial Baud Rate
#define SERIAL_BAUD_RATE        115200

// Inactivity Sleep Timeout (Auto-dim screen after no presence)
#define SLEEP_TIMEOUT_MS        30000

#endif // TRUSTBOX_CONFIG_H
