# Paytm TrustBox: Hardware Setup & Wiring Guide

This guide provides complete instructions for wiring, configuring, and testing the physical hardware for the **Paytm TrustBox** terminal.

---

## 1. Bill of Materials (BOM)

| Component | Recommended Model / Specification | Purpose in TrustBox |
|---|---|---|
| **Microcontroller** | ESP32 DevKit V1 (30-pin or 38-pin, ESP-WROOM-32) | Core processor: Wi-Fi, UI, audio, crypto & backend comms |
| **Display** | 3.2" SPI TFT LCD (ILI9341 driver, 240x320) with Touch (XPT2046) | Shows payment status, QR code, warnings, and touch UI |
| **Microphone** | INMP441 (I2S digital) or MAX9814 (analog with auto-gain) | Captures voice queries ("Kitna aaya?", "Check payment") |
| **Audio Output** | MAX98357A I2S DAC Amp + 3W 4Ω Speaker (or active Piezo Buzzer) | Speaks payment confirmation (Hindi/English TTS) and alert chime |
| **Dispute Button** | Momentary Pushbutton (Red / Triangle 🔺 cap) | Merchant dispute & instant verification trigger |
| **Verify Button** | Momentary Pushbutton (Green / Check mark) | Secondary quick-verify trigger |
| **Motion Sensor** | PIR Motion Sensor (HC-SR501) or IR Obstacle Sensor (FC-51) | Wake-on-presence: wakes screen/terminal when merchant approaches |
| **Status Indicator** | Common Cathode RGB LED (with 3x 220Ω resistors) | Visual state indicator (Green = OK, Yellow = Verifying, Red = Fraud) |
| **Power Supply** | 5V / 2A Micro-USB or Type-C adapter | Reliable power for ESP32 + TFT backlight + speaker amp |

---

## 2. ESP32 Pin Assignment Table

Standard pin allocation configured in `config.h`:

| ESP32 GPIO | Module / Component | Pin on Module | Function / Signal |
|---|---|---|---|
| **3V3 / VIN** | All 3.3V / 5V rails | VCC / VDD | Power (TFT VCC to 3.3V or 5V depending on module) |
| **GND** | All Ground rails | GND | Common ground reference |
| **GPIO 18** | 3.2" TFT LCD | SCK / CLK | SPI Clock |
| **GPIO 23** | 3.2" TFT LCD | MOSI / SDI | SPI Master Out Slave In |
| **GPIO 19** | 3.2" TFT LCD | MISO / SDO | SPI Master In Slave Out (optional) |
| **GPIO 15** | 3.2" TFT LCD | CS | TFT Chip Select |
| **GPIO 2** | 3.2" TFT LCD | DC / RS | Data / Command Select |
| **GPIO 4** | 3.2" TFT LCD | RESET | TFT Reset |
| **GPIO 21** | 3.2" TFT LCD | LED / BL | Backlight PWM control (or tie to 3.3V) |
| **GPIO 5** | XPT2046 Touch | T_CS | Touch Chip Select |
| **GPIO 27** | PIR / IR Sensor | OUT | Digital input (Interrupt on HIGH) |
| **GPIO 13** | Dispute Button (🔺) | Pin 1 (Pin 2 to GND) | Internal PULLUP, active LOW |
| **GPIO 12** | Verify Button | Pin 1 (Pin 2 to GND) | Internal PULLUP, active LOW |
| **GPIO 25** | Audio / Buzzer | DIN (I2S) or (+) Buzzer | Audio signal / PWM tone |
| **GPIO 26** | Audio Amp (I2S) | LRC / WS | Word Select (Left/Right Clock) |
| **GPIO 22** | Audio Amp (I2S) | BCLK | Bit Clock |
| **GPIO 14** | RGB LED | Red Anode | 220Ω series resistor |
| **GPIO 32** | RGB LED | Green Anode | 220Ω series resistor |
| **GPIO 33** | RGB LED | Blue Anode | 220Ω series resistor |
| **GPIO 34** | Mic (INMP441 / Analog)| SD (I2S) or OUT (Analog) | Voice input stream |

---

## 3. Wiring Schematics & Connections

### A. 3.2" TFT Display (SPI ILI9341)
```
ESP32                    ILI9341 TFT Display
+3.3V / 5V  -----------> VCC
GND         -----------> GND
GPIO 15     -----------> CS (Chip Select)
GPIO 4      -----------> RESET
GPIO 2      -----------> DC / RS
GPIO 23     -----------> SDI / MOSI
GPIO 18     -----------> SCK / CLK
GPIO 21     -----------> LED (Backlight control)
GPIO 19     <----------- SDO / MISO
```

### B. Pushbuttons (Debounced Internal Pullups)
```
ESP32 GPIO 13 ----------> [ Dispute Button 🔺 ] ----------> GND
ESP32 GPIO 12 ----------> [ Verify Button  ] ----------> GND
```

### C. IR / PIR Presence Sensor
```
ESP32 +3.3V / 5V -------> PIR VCC
ESP32 GND         -------> PIR GND
ESP32 GPIO 27     <------- PIR OUT
```

### D. RGB Status LED (Common Cathode)
```
ESP32 GPIO 14 ---> [ 220Ω ] ---> Red Anode   |
ESP32 GPIO 32 ---> [ 220Ω ] ---> Green Anode | Common Cathode ---> GND
ESP32 GPIO 33 ---> [ 220Ω ] ---> Blue Anode  |
```

### E. Audio Output (MAX98357A I2S or Buzzer)
For MAX98357A I2S Amplifier:
```
ESP32 GPIO 22 ---> BCLK
ESP32 GPIO 26 ---> LRC
ESP32 GPIO 25 ---> DIN
ESP32 5V      ---> VIN
ESP32 GND     ---> GND
```
*(Or for rapid prototype Piezo Buzzer: GPIO 25 to positive leg via 100Ω resistor, negative leg to GND).*

---

## 4. Flashing Instructions

### Using Arduino IDE
1. Install **ESP32 Board Support** via Boards Manager:
   - URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Install Required Libraries via Library Manager:
   - `ArduinoJson` (v6 or v7)
   - `TFT_eSPI` or `Adafruit GFX Library` + `Adafruit ILI9341`
   - `HTTPClient` & `WiFi` (built into ESP32 core)
3. Open `trustbox_firmware.ino` in Arduino IDE.
4. Update `config.h` with your local Wi-Fi SSID, Password, and Backend Server IP.
5. Select Board: **ESP32 Dev Module**, Upload Speed: **921600**, Flash Frequency: **80MHz**.
6. Hold **BOOT** button on the ESP32 when `Connecting...` appears in terminal, then release.

---

## 5. Serial Diagnostic Interface

Even before connecting to the backend or when testing components on the breadboard, you can open Serial Monitor at **115200 baud** and send test commands:

| Command | Action |
|---|---|
| `STATUS` | Prints hardware status, WiFi connection, and pin readings |
| `WAKE` | Simulates IR motion detection event |
| `VERIFY` | Simulates pressing the Verify button |
| `DISPUTE` | Simulates pressing the 🔺 Dispute button |
| `TEST_LED` | Cycles RGB LED through Red, Yellow, Green |
| `TEST_TONE` | Plays success chime and fraud alert siren |
| `TEST_DISPLAY` | Cycles TFT screens: Home, Verifying, Success, Fraud Alert |
| `SET_MID <id>` | Dynamically sets Merchant ID (default: `M12345678`) |

This allows instant, hands-on validation while building!
