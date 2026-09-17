# Paytm TrustBox: Fraud-Proof UPI Verification Device

**Executive Summary:** *Paytm TrustBox* is a voice-driven, AI-enhanced hardware terminal built on the Paytm Soundbox to give merchants two-way payment verification and fraud protection. Designed for low-literacy Tier-2/3 shopkeepers, TrustBox lets a merchant query or dispute a payment (“Kitna aaya?” or press a button), then checks Paytm’s backend and returns a clear voice/display answer. Unlike Soundbox v1 (which only plays an arrival chime), TrustBox actively analyzes transactions via Paytm APIs, computes a fraud risk score, and responds via TTS and an OLED display. This boosts merchant trust and retention (by cutting scam losses) while generating upsell revenue (a premium ₹149/m plan over the ₹125/m standard). In short, TrustBox combines on-device sensing (IR, mic, buttons) and backend AI to reliably confirm genuine payments and flag fraud attempts.

## Problem Statement

Small Indian merchants suffer widespread UPI fraud: **₹1.7 billion** (USD 1.7B) was lost to UPI scams in 2023. Social-engineering attacks exploit merchants’ lack of digital training. Common scams include: 

- **Fake Payment Screenshot:** A fraudster presents a doctored “Payment Successful” screen (e.g. a fake PhonePe/Paytm screenshot) to claim they paid. The merchant hands over goods before realizing the money never arrived.  
- **Spoofed Sound Alerts:** Criminals install fake UPI apps that *play* a payment chime on the shop’s speaker, mimicking the Paytm Soundbox tone, even though no payment occurred. (By design a genuine Soundbox only responds to Paytm’s server, so any sound from the customer’s phone is fake audio.)  
- **Refund/Overpayment Scam:** The buyer claims to have overpaid or paid the wrong shop and immediately asks for a cash refund. Since no real payment happened, the refund is pure profit for the scammer.  
- **Social Engineering/Distraction:** Fraudsters work in teams – one engages the cashier while others surreptitiously simulate payment. Attacks often happen when the owner is absent or at a busy time, exploiting staff unfamiliarity.  

These schemes drive distrust and merchant churn. NPCI and industry reports emphasize **never trusting the customer’s screen or audio** alone. Merchants need an on-site device that independently verifies each UPI credit and warns of anomalies.

## Target Users & Personas

TrustBox targets **Tier-2/3 merchants with low digital literacy**. Examples:  

- *Rajesh (Age 45)* – Kirana shop owner in a small town. Uses Paytm only via an agent’s Soundbox; can’t read English. Fears new payment apps, so easily duped by quick audio cues. He needs voice prompts in Kannada/Hindi and very simple interface.  
- *Pooja (Age 32)* – Works in a roadside cafe. Semi-literate, primarily cash-based. Overwhelmed when busy: trusting quick buyer reassurances (“Jackpot, customer shows screen, we hand over goods”). Needs foolproof verification (voice output, clear text, and button to double-check) and alerts if something seems off.  

Both rely on clear Hindi/English voice TTS and icons rather than text. They benefit from training (e.g. a sticker: “Payment only confirmed by device”), but TrustBox handles the technical verification so they need minimal app knowledge.

## Core Features & User Flows

TrustBox follows the **Sense → Connect → Investigate → Predict → Act** pipeline:

- **Sense:**  The device idles (screen showing logo or static QR). An **IR sensor** or touch **button** or voice command wakes it. E.g. merchant presses the red “🛠” button or says “Kitna aaya?” (“How much came?”). *Inputs:* motion, button press, or audio. *Outputs:* an event trigger and captured audio. *Latency:* immediate (<0.5s for wake-up). *Failure modes:* False wake (ambient noise), voice misrecognition (“Bolna kya aaya?” misheard). *UX:* On wake, screen shows “Verifying payment…” and an hourglass icon; speaker stays quiet until response.  

- **Connect:**  The ESP32 forms a JSON payload and sends it via secure Wi-Fi to the backend. *Inputs:* merchant ID, timestamp, (optionally merchant-provided transaction reference or amount). For simplicity, TrustBox can tag the *latest* expected order ID (e.g. from dynamic QR or merchant app) or ask merchant to tap the button right after showing the QR code. *Outputs:* HTTP request to server (e.g. `POST /api/verify`). *Latency target:* <200ms to server. *Failure:* No network or timeouts. *UX:* On failure, the display shows “Network error. Try again” and the speaker says, “Unable to connect; check your network.”  

- **Investigate:**  The backend (using n8n workflow) uses the audio transcript (“Kitna aaya?” or “Check payment”) and/or associated data to identify the transaction. It calls Paytm’s APIs (using test keys in sandbox) to get transaction details for the merchant (using merchant’s MID and last order ID). *Inputs:* merchant ID, query event. *Outputs:* transaction record or “not found”. *Latency:* ~1–2s. *Failure:* Paytm API down (timeout); no matching transaction. *UX:* If no data returned, device says “No payment found” and shows ⚠️ icon.  

- **Predict:**  A fraud-detection module (on server) scores the transaction. Inputs include: transaction amount, merchant’s history, buyer cluster data (via a merchant graph), frequency of disputes, etc. E.g. high fraud if (amount ≠ recorded, merchant never saw it). It outputs a score (0–1). *Latency:* ~100ms. *Failure:* Model error; fallback is rule-based (e.g. if no record, flag 100% fraud). *UX:* If score > threshold, mark “suspect”.  

- **Act:**  Based on status and score, TrustBox responds. *Outputs:*  TTS voice (in selected language) and TFT text. For success: “Payment of ₹\<amount\> confirmed”; screen shows “₹\<amount\> received” with green check. For no-payment: “Transaction not found” (and display “No incoming payment” with red X). For high fraud-score: “⚠️ Warning: Possible fraudulent transaction detected” (plus flashing LED/beep). For disputes: if merchant pressed the dispute button, device confirms “Dispute raised – contacting support”. *Latency:* <1s from backend response. *Failure:* Display not working, or voice misplayed; then default beep or LED.  

Throughout, the device UX is **guided and redundant**: voice and text correspond. All replies end with a prompt (“Need to dispute? Press 🔺 again.”) if unresolved, to guide the user.  

## Hardware Mapping

| Sensor/Module       | Anti-Fraud Role & Firmware                                                           |
|---------------------|---------------------------------------------------------------------------------------|
| **ESP32 MCU (Wi-Fi)**   | Main processor. Runs firmware (C/C++/MicroPython) for UI, audio, comms. Handles network calls, sensor polling, and TTS generation.  |
| **3.2″ TFT Touch**     | Displays UI screens: static/dynamic QR (v2 feature), transaction status, prompts (e.g. “Verifying payment…”, amounts, warnings). Also a touch interface for menu/settings. Firmware module: graphics library with font rendering (Hindi/English), touchscreen driver. |
| **Microphone**        | Captures merchant voice queries (“Kitna aaya?”). Firmware includes voice-trigger detection and audio capture. Audio streamed to backend (via Sarvam STT) for intent recognition. Could also detect background sound anomalies (e.g. loud spoofed chime). |
| **Speaker (or BT Speaker)**| Plays TTS responses from server (Android “Sarvam” voice). Also plays alert sounds (beeps/LED chimes). In fraud mode, can emphasize alerts. Firmware: audio decoder/TTS driver. |
| **Touch Button (🔺)**   | Manual trigger: merchant presses when raising a dispute/verification. Debounce logic in firmware. Also used to select menu options (e.g. scrolling through settings on the TFT). |
| **IR Motion Sensor**  | Wake-on-presence: conserves power by sleeping display/audio until merchant approaches. Can also detect if a customer leans in or obstructs (security camera future). Firmware: simple digital input interrupt. |
| **LED/Buzzer (Status)**| Multi-color LED or buzzer signals status: green=success, red=alert, yellow=processing. Provides non-verbal cues. Firmware toggles based on event stage. |
| **(Optional) RFID Reader** | Could read merchant’s NFC card (e.g. employee badge) to authorize dispute actions. Or in v2, allow scanning customer loyalty card for better fraud detection. Firmware: communicates via SPI/I2C. |
| **Unused Sensors (Moisture/Gas/Others)** | Not directly relevant to payments. (Could repurpose a gas sensor for environment alerts, but out-of-scope.) Arduino Uno is redundant if ESP32 suffices. |

Each hardware component runs a firmware module (e.g. `audio.c`, `display.c`, `network.c`, `fraudcheck.c`). The ESP32 handles all sensors and communicates securely with the backend.

## Network & Backend Design

- **Event Payload (Device→Server):** Each query/dispute is sent as JSON over HTTPS. Example:  
  ```json
  {
    "event": "verify_payment",
    "merchantId": "M12345678",
    "timestamp": "2026-09-17T05:00:00Z",
    "orderId": "ORD5555"
  }
  ```  
  If using voice, include `"query": "Kitna aaya?"` or transcribed amount.  

- **Paytm API Verification:** The backend uses Paytm’s Payment Gateway or UPI APIs to verify the transaction. For example, it might call a “Check Transaction Status” endpoint with the merchant’s MID and orderID. (During development we use Paytm’s sandbox/test merchant keys.) The backend also consults the merchant’s Paytm account via test APIs or mock data to confirm amounts. 

- **Authentication & Security:** All communication uses TLS. The device authenticates with an API key or client cert provisioned at installation. Paytm API calls use the merchant’s credentials (checksum key or OAuth token as per Paytm docs). The backend verifies the device’s credentials (e.g. signed HMAC) on each request. 

- **Retry & Backoff:** If a network or API call fails, the ESP32 retries up to 3 times with exponential backoff (0.5s, 1s, 2s). On persistent failure, it informs the merchant (display “Retry failed – check network”). Sensitive actions (like dispute filing) require a successful response or manual operator intervention.

- **Escalation Hooks:** The backend automatically **escalates** any flagged fraud to operations. For example, if `fraudScore > 0.8`, it creates a ticket in Paytm’s ops system (e.g. via n8n) with details. A text/email alert may be sent to risk analysts. The device might also be configured to email the merchant a receipt or code to file an official complaint. All events are logged for compliance. (Merchants are advised to file a cybercrime report via 1930 if fraud is confirmed.)

## ML / Heuristics

TrustBox’s fraud score combines learned and rule-based signals:  

- **Features:** Transaction amount, time of day, merchant’s sales history, buyer’s past behavior, cluster patterns (via Cognee graph). E.g. if a buyer’s UPI ID has “tried” to pay multiple shops with only fake alerts, or if the same group of people hit nearby stalls. Also context (very large or round-figure amount, odd timings) as NPCI’s federated model does.  

- **Model:** A simple lightweight ML model (e.g. logistic regression or decision tree) runs on the backend. It might use an embedding of the merchant graph. For explainability, features are logged (e.g. “no matching transaction” or “repeat refund”).  

- **Rule Fallbacks:** Hard rules supplement ML. For instance: *If* Paytm API returns *no record*, then fraudScore=1.0; *if* the customer’s app ID is known to be fresh or flagged, increment score. *If* a prior “refund” was requested today, warn.  

- **Thresholds & Explainability:** We define tiers: score<0.3=green (safe), 0.3–0.7=yellow (caution), >0.7=red. Explanations (for ops) can cite the main factor: e.g. “Unknown transaction” or “High-risk buyer”. These may be logged and optionally spoken: e.g. “Amount differs from last payment” in a subtle tone.

## Dashboard & Alerts

TrustBox includes a simple web dashboard for Paytm operations:

- **Views:**  
  - *Live Dashboard:* Real-time feed of all device events (time, merchant ID, amount, score).  
  - *Fraud Alerts:* List of flagged transactions (score >0.7) with merchant name, location, time and details. Allows Ops to acknowledge or dismiss.  
  - *Dispute Queue:* Active merchant disputes (those who pressed the button) pending resolution. Includes merchant info and last message.  
  - *Analytics:* Metrics such as reduction in merchant complaints (churn rate), false-positive rate, mean time to resolution, and subscription uptake.  

- **Sample Wireframe Table:**  

  | Dashboard Section | Contents                             | Key Metrics/Functions                    |
  |-------------------|--------------------------------------|------------------------------------------|
  | Live Transactions | Timestamp, Merchant, Txn ID, Amount   | Filter by time/merchant                  |
  | Fraud Alerts      | Merchant, Time, Amount, Score, Status | *Flagged:* (High risk) with reason      |
  | Dispute Queue     | Merchant, Query Time, Text, Status    | Escalate to support team                 |
  | Metrics & Reports | Charts of churn reduction, FP rate   | e.g. “Frauds prevented vs baseline”      |

- **Mermaid Timeline (Event Flow):** A sample sequence of a payment verification event:

  ```mermaid
  sequenceDiagram
    participant Merchant
    participant TrustBox
    participant Backend
    Merchant->>TrustBox: Press “Verify” or say query
    TrustBox->>Backend: Send {merchantId, orderId} JSON
    Backend->>Paytm API: Check transaction status
    Paytm API-->>Backend: {status: success/failed, amount}
    Backend->>Backend: Compute fraud score (ML/Rules)
    Backend->>TrustBox: Return {status, amount, fraudScore}
    TrustBox-->>Merchant: Speak/display result (confirm or warn)
  ```

## TFT UI Mockups & Voice Prompts

The 3.2″ screen presents clear text and icons; the speaker uses polite Hindi/English (or local language):

- **Idle/Home Screen:** Shows merchant logo and optionally the static QR. Prompt at bottom: “*Say “Kitna aaya?” or press Verify.*”  
- **Verifying Screen:** Upon query, display “Verifying transaction… Please wait.” (hourglass icon). *TTS:* “Verifying payment status.”  
- **Success Screen:** “Payment received: ₹500” with green checkmark and time. *TTS:* “₹500 has been received in your account.”  
- **Not Found Screen:** “No payment found.” with red cross. *TTS:* “No payment record found for that transaction.”  
- **Fraud Alert:** If flagged, display “⚠️ Possible fraud! Press Dispute.” *TTS:* “Warning: this transaction looks suspicious. Please verify carefully or press the Dispute button.”  
- **Dispute Flow:** When merchant presses dispute, confirm: “Dispute raised.” *TTS:* “Dispute registered. Please follow up with support.”  
- **Settings Screen:** Simple menu to change language, test mode, subscription status, with touch options.

All on-screen text is short (3–5 words) and in large font. Voice messages are friendly, e.g. **“Paytm TrustBox: ₹\<amt\> confirmed”**, or **“Koi payment nahin mila”** if in Hindi.

## Data Schema & APIs

**Example JSON (Device→Backend):**  
```json
{
  "eventType": "verify_payment",
  "merchantId": "M12345678",
  "timestamp": "2026-09-17T11:05:00Z",
  "orderId": "ORD5555",
  "payload": { "amount": 250 }
}
```

**Backend Response:**  
```json
{
  "status": "confirmed",
  "amount": 250,
  "time": "2026-09-17T11:05:02Z",
  "fraudScore": 0.10,
  "message": "Payment confirmed"
}
```

**API Endpoints:**  
- `POST /api/verify_payment` – Verifies a payment (called on Sense→Connect).  
- `POST /api/report_dispute` – Logs a merchant dispute; may trigger ops alert.  
- All responses include `{status, data, fraudScore}`.  

Fields like `merchantId`, `orderId`, `amount`, `timestamp` are strings/numbers. Fraud score is a float 0–1. Response codes: 200 (OK), 404 (txn not found), 500 (server error).

## Test Plan & Demo Script

1. **Setup:** Register test merchant account on Paytm sandbox. Configure TrustBox with sandbox credentials (MerchantID, API keys). Verify device connects (green LED).  
2. **Scenario A – Legitimate Payment:** Use Paytm sandbox or UPI test app to send ₹100 to the merchant (static/dynamic QR). Device should chime and display **“Payment received: ₹100”**. User queries “Kitna aaya?” and sees ₹100 confirmed. *KPI:* Response ≤3s, correct amount.  
3. **Scenario B – Fake Screenshot:** Simulate a fake payment: do not actually send money, but let a “customer” present a phone saying “₹100 sent”. Merchant presses verify. System finds no record, and **flags** it. Device should display **“No payment found”** (with red alert) and voice a warning. *KPI:* FraudScore triggered, no false confirmation.  
4. **Scenario C – Fake Sound:** Play a prerecorded Paytm chime near the device without a real transaction. The TrustBox should ignore it (no network event) and say nothing. *KPI:* No false alarms (only genuine queries prompt action).  
5. **Scenario D – Network Down:** Disable Wi-Fi and attempt verify. Device retries, then shows network error. *KPI:* Proper error message.  
6. **Metrics:** Track detection accuracy (should catch 100% of our fake tests), false positives (should be 0% on honest payments), average verify time (target <3s), and user satisfaction (qualitative).  

**Demo Script:** Operator walks merchant through scanning or query, shows actual bank update vs device readout, and then a staged scam (no credit happens, device blocks it). Use Paytm’s sandbox UTR numbers to demo API calls. Include a screen for the dashboard showing the alert list populating in real time.

## Roadmap, Milestones & Risks

- **Q4 2026:** *Proof of Concept* – Build a prototype: integrate ESP32 with display, mic, button, and basic firmware to send fixed payloads. 1 HW engineer, 1 software engineer.  
- **Q1 2027:** *Backend & AI* – Develop server workflows (n8n, Sarvam STT), integrate Paytm test APIs. Implement fraud scoring prototype. 1 backend dev, 1 data scientist.  
- **Q2 2027:** *Integration & UX* – Build full device firmware: voice recording, TTS, UI screens. Develop dashboard with alerts/logs. 1 UX engineer, test with small merchant pilot.  
- **Q3 2027:** *Beta & Iteration* – Pilot with live Paytm accounts. Measure KPIs (fraud catch rate, merchant feedback). Refine ML model and interfaces.  
- **Q4 2027:** *Launch* – Scale manufacturing, begin sales.  

**Resources:** Estimated 4–5 FTEs over 9 months. Hardware cost ~$30 per unit (ESP32 dev-kit, display, sensors). Cloud/DB hosting moderate. Integration with Paytm (dev support needed).

**Risks:**  
- *False Positives:* Overzealous scoring could annoy merchants. Mitigate with human review (dashboard) and explainable alerts.  
- *Connectivity:* Rural networks are spotty. Offline caching of last N transactions and retry logic can help.  
- *User Adoption:* Merchants may resist new device. Counter with training (on-device prompts, trader-group outreach) and easy UI.  
- *Regulatory:* Must comply with NPCI and RBI guidelines. We assume basic API access is allowed; if not, work with Paytm.

## Monetization & Escalation Policy

- **Subscription Upsell:** TrustBox is offered as a *premium* add-on. Merchants can rent standard Soundbox at ₹125/month; TrustBox (with AI verification) is ₹149/month. The extra ₹24/m covers the added services (fraud alerts, support). Over 2 years, this upsell yields ~20% higher ARPU while dramatically reducing fraud claims and churn. Additional revenue can come from value-added services (e.g. targeted loan referrals based on transaction logs).  

- **Churn Prevention:** By lowering scam losses, TrustBox helps retain merchants who might otherwise leave Paytm. (Even anecdotal feedback suggests many shopkeepers blame Paytm for payment issues.) Reduced attrition means more stable subscription base. 

- **Operations Escalation:** Any transaction flagged as high-risk automatically notifies Paytm’s support team. We will coordinate with Paytm to define an escalation protocol: likely, urgent review by fraud analysts and, if confirmed, assistance to the merchant (e.g. advising them to file an NPCI complaint). We also advise encouraging merchants to report confirmed fraud via official channels (NCAC/1930). Over time, a shared fraud database can be updated (e.g. blacklisting repeat offenders). TrustBox logs (with timestamps, audio, transcripts) serve as evidence if disputes arise.

**Sources:** The design draws on Paytm’s own guidance (Paytm Developer docs for APIs), NPCI rules, and industry fraud research. For example, NPCI and analysts stress *never* trusting customer screenshots, and PhishGuard notes that **voice alerts can be spoofed** (soundbox scams). These insights, plus UPI fraud stats, shaped the above requirements. All recommendations align with NPCI’s fraud-control best practices.  

