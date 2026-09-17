# Paytm TrustBox: Fraud-Proof UPI Verification Terminal

**Paytm TrustBox** is a two-way payment verification and anti-fraud hardware terminal built on the Paytm Soundbox concept. Designed for low-literacy Tier-2/3 merchants, TrustBox enables merchants to query or dispute payments (*"Kitna aaya?"* or press the red 🔺 button), automatically verifies status against Paytm's UPI Gateway, calculates an AI/heuristic fraud risk score, and delivers clear multilingual voice (Hindi/English) and TFT visual alerts.

---

## System Architecture

```
[ Merchant / Customer ]
       │
       ▼
[ Hardware Terminal (ESP32) ]
  ├── 3.2" SPI TFT LCD (ILI9341) - High contrast Hindi/English UI
  ├── Pushbuttons - 🔺 Dispute (Pin 13) & Verify (Pin 12)
  ├── I2S / Analog Mic (Pin 34) - Voice queries ("Kitna aaya?")
  ├── Speaker / Buzzer (Pin 25) - Official chime & fraud siren
  ├── IR Motion Sensor (Pin 27) - Wake-on-presence
  └── RGB LED (Pins 14,32,33) - Green (Safe), Yellow (Verifying), Red (Fraud)
       │
       ▼ (HTTPS / JSON over Wi-Fi)
[ TrustBox Backend Server (Node.js Express) ]
  ├── Paytm Payment Gateway Adapter (Sandbox & Ledger)
  ├── ML & Heuristic Fraud Detection Engine (Scoring 0.00 - 1.00)
  ├── Multilingual Speech & Intent Engine (Hindi & English)
  └── Real-Time SSE Event Broadcaster
       │
       ▼
[ Paytm Operations & Merchant Web Portal ]
  ├── Interactive Hardware Terminal Simulator & GPIO Monitor
  ├── PRD Test Scenario Studio (Scenarios A, B, C, D)
  ├── Live Verification Stream
  ├── High-Risk Fraud Alerts & Ops Escalation (NPCI / 1930 Cybercrime)
  └── Merchant Dispute Queue (🔺 Button Tickets)
```

---

## Quick Start Guide

### 1. Start the Backend Server & Dashboard

```bash
cd a:\trustbox\backend
node server.js
```

Once started, open your web browser at:
**`http://localhost:3000`**

This launches the **Paytm Operations Portal & Live Hardware Simulator**, where you can immediately interact with the virtual TrustBox terminal, test speech queries, trigger disputes, and execute test scenarios while your physical hardware is being set up.

---

### 2. Run the Automated Test Suite

```bash
cd a:\trustbox\backend
npm test
```

Runs all 12 test assertions verifying:
- **Scenario A**: Legitimate Payment (₹100) $\rightarrow$ Confirmed, $\text{Score} < 0.30$
- **Scenario B**: Fake Payment Screenshot $\rightarrow$ No Gateway Record $\rightarrow$ Flagged $\text{Score} = 1.00$
- **Scenario C**: Spoofed Sound Alert $\rightarrow$ Acoustic Anomaly $\rightarrow$ Flagged $\text{Score} \ge 0.95$
- **Rule 2**: Amount Discrepancy Scam (Claim ₹1000, Paid ₹10) $\rightarrow$ $\text{Score} \ge 0.85$
- **Voice Queries**: Intent parsing and Hindi/English speech synthesis

---

### 3. Flashing Physical ESP32 Hardware

The `firmware/` directory contains the complete modular Arduino / C++ code:

1. Open `firmware/trustbox_firmware.ino` in the Arduino IDE.
2. Ensure you have installed:
   - `ArduinoJson` (v6 or v7)
   - `ESP32` board package
3. Configure your Wi-Fi SSID and backend server IP in `firmware/config.h`:
   ```cpp
   #define DEFAULT_WIFI_SSID   "Your_WiFi_SSID"
   #define DEFAULT_WIFI_PASS   "Your_WiFi_Password"
   #define DEFAULT_SERVER_URL  "http://<YOUR_PC_IP>:3000"
   ```
4. Consult **`firmware/HARDWARE_SETUP.md`** for the complete pinout table, schematics, and wiring instructions for:
   - ILI9341 3.2" SPI TFT LCD (Pins 15, 2, 4, 23, 18, 19, 21)
   - Dispute Button 🔺 (Pin 13) & Verify Button (Pin 12)
   - IR Motion Sensor (Pin 27)
   - Audio DAC / Buzzer (Pin 25)
   - Common Cathode RGB LED (Pins 14, 32, 33)
   - Microphone (Pin 34)

5. Open Serial Monitor at **115200 baud** to access the built-in diagnostic CLI (`STATUS`, `WAKE`, `VERIFY`, `DISPUTE`, `TEST_LED`, `TEST_AUDIO`).
