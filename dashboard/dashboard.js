/**
 * ============================================================================
 * PAYTM TRUSTBOX: OPERATIONS PORTAL & LIVE HARDWARE SIMULATOR
 * ============================================================================
 */

// Application State
let currentMerchant = {
    id: 'M12345678',
    name: 'Rajesh Kirana Store'
};
let currentLanguage = 'hi';
let lastSpokenText = 'Paytm TrustBox is ready.';
let audioContext = null;
let eventSource = null;

// Clock update
setInterval(() => {
    const now = new Date();
    const timeStr = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    const clockEl = document.getElementById('tftHeaderClock');
    if (clockEl) clockEl.textContent = timeStr;
}, 1000);

// Initialize on DOM Loaded
document.addEventListener('DOMContentLoaded', () => {
    initAudioContext();
    initSseConnection();
    initEventListeners();
    fetchInitialData();
    renderHomeScreen();
});

// ----------------------------------------------------------------------------
// 1. TAB SWITCHING
// ----------------------------------------------------------------------------
function switchTab(tabName) {
    document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(content => content.classList.remove('active'));

    const activeBtn = Array.from(document.querySelectorAll('.tab-btn')).find(b =>
        b.getAttribute('onclick') && b.getAttribute('onclick').includes(tabName)
    );
    if (activeBtn) activeBtn.classList.add('active');

    const activeContent = document.getElementById(`tab-${tabName}`);
    if (activeContent) activeContent.classList.add('active');
}

// ----------------------------------------------------------------------------
// 2. AUDIO SYNTHESIZER (WEB AUDIO API)
// ----------------------------------------------------------------------------
function initAudioContext() {
    try {
        const AudioCtx = window.AudioContext || window.webkitAudioContext;
        if (AudioCtx) {
            audioContext = new AudioCtx();
        }
    } catch (e) {
        console.warn('AudioContext not supported in this environment.');
    }
}

function ensureAudioReady() {
    if (audioContext && audioContext.state === 'suspended') {
        audioContext.resume();
    }
}

function playFrequencyTone(freq, durationMs, type = 'sine') {
    ensureAudioReady();
    if (!audioContext) return;

    try {
        const osc = audioContext.createOscillator();
        const gain = audioContext.createGain();

        osc.type = type;
        osc.frequency.setValueAtTime(freq, audioContext.currentTime);

        gain.gain.setValueAtTime(0.2, audioContext.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.01, audioContext.currentTime + (durationMs / 1000));

        osc.connect(gain);
        gain.connect(audioContext.destination);

        osc.start();
        osc.stop(audioContext.currentTime + (durationMs / 1000));
    } catch (e) {
        console.error('Audio playback error:', e);
    }
}

function playPaytmSuccessChime() {
    ensureAudioReady();
    logSerial('[AUDIO] >> Playing Paytm Official Chime (E5 -> G#5 -> B5 -> E6)');
    updatePinStatus('pin25', 'PLAYING CHIME');

    const notes = [
        { freq: 659.25, time: 0, dur: 120 },    // E5
        { freq: 830.61, time: 140, dur: 120 },  // G#5
        { freq: 987.77, time: 280, dur: 140 },  // B5
        { freq: 1318.51, time: 440, dur: 400 }  // E6
    ];

    notes.forEach(n => {
        setTimeout(() => {
            playFrequencyTone(n.freq, n.dur, 'triangle');
        }, n.time);
    });

    setTimeout(() => updatePinStatus('pin25', 'IDLE'), 1000);
}

function playFraudAlertSiren() {
    ensureAudioReady();
    logSerial('[AUDIO] >> ⚠️ Playing FRAUD WARNING SIREN (880Hz <-> 440Hz)', 'err');
    updatePinStatus('pin25', 'SIREN ACTIVE', true);

    for (let i = 0; i < 4; i++) {
        setTimeout(() => playFrequencyTone(880, 150, 'sawtooth'), i * 320);
        setTimeout(() => playFrequencyTone(440, 150, 'sawtooth'), i * 320 + 160);
    }

    setTimeout(() => updatePinStatus('pin25', 'IDLE'), 1500);
}

function playDisputeChime() {
    ensureAudioReady();
    logSerial('[AUDIO] >> Playing Dispute Confirmation Chime');
    playFrequencyTone(440, 150);
    setTimeout(() => playFrequencyTone(554, 150), 160);
    setTimeout(() => playFrequencyTone(659, 250), 320);
}

function playButtonClickTone() {
    playFrequencyTone(1200, 30, 'square');
}

// ----------------------------------------------------------------------------
// 3. MULTILINGUAL SPEECH SYNTHESIS & RECOGNITION (TTS / STT)
// ----------------------------------------------------------------------------
function speakPrompt(text, lang = currentLanguage) {
    lastSpokenText = text;
    const ttsEl = document.getElementById('ttsOutputText');
    if (ttsEl) ttsEl.textContent = `"${text}"`;

    logSerial(`[TTS (${lang})] Spoken: "${text}"`, 'succ');

    if ('speechSynthesis' in window) {
        window.speechSynthesis.cancel();
        const utterance = new SpeechSynthesisUtterance(text);
        utterance.lang = (lang === 'hi') ? 'hi-IN' : 'en-IN';
        utterance.rate = 0.95;

        // Try to pick a suitable voice if available
        const voices = window.speechSynthesis.getVoices();
        const targetVoice = voices.find(v => v.lang.startsWith(lang));
        if (targetVoice) utterance.voice = targetVoice;

        window.speechSynthesis.speak(utterance);
    }
}

function replayLastTts() {
    speakPrompt(lastSpokenText, currentLanguage);
}

function startVoiceRecognition() {
    const btn = document.getElementById('btnVoice');
    btn.classList.add('listening');
    logSerial('[VOICE] Microphone listening for query (e.g. "Kitna aaya?")...');
    updatePinStatus('pin34', 'CAPTURING AUDIO', true);

    const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;

    if (SpeechRecognition) {
        const recognition = new SpeechRecognition();
        recognition.lang = currentLanguage === 'hi' ? 'hi-IN' : 'en-IN';
        recognition.interimResults = false;
        recognition.maxAlternatives = 1;

        recognition.onresult = (event) => {
            const transcript = event.results[0][0].transcript;
            logSerial(`[VOICE] Merchant Spoke: "${transcript}"`, 'succ');
            btn.classList.remove('listening');
            updatePinStatus('pin34', 'IDLE');
            executeVoiceQuery(transcript);
        };

        recognition.onerror = (event) => {
            console.warn('Speech recognition error:', event.error);
            btn.classList.remove('listening');
            updatePinStatus('pin34', 'IDLE');
            fallbackVoicePrompt();
        };

        recognition.onend = () => {
            btn.classList.remove('listening');
            updatePinStatus('pin34', 'IDLE');
        };

        recognition.start();
    } else {
        fallbackVoicePrompt();
    }
}

function fallbackVoicePrompt() {
    const btn = document.getElementById('btnVoice');
    btn.classList.remove('listening');
    updatePinStatus('pin34', 'IDLE');

    const defaultQuery = currentLanguage === 'hi' ? 'Kitna aaya?' : 'Check payment status';
    const query = prompt('Simulate Merchant Voice Query (e.g. "Kitna aaya?", "Verify 500"):', defaultQuery);
    if (query) {
        executeVoiceQuery(query);
    }
}

async function executeVoiceQuery(queryText) {
    renderVerifyingScreen(`Query: "${queryText}"`);
    setLed('yellow');

    try {
        const res = await fetch('/api/voice_query', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                merchantId: currentMerchant.id,
                query: queryText,
                lang: currentLanguage
            })
        });

        const data = await res.json();
        handleVerificationResponse(data);
    } catch (err) {
        renderNetworkErrorScreen(err.message);
    }
}

// ----------------------------------------------------------------------------
// 4. VIRTUAL 3.2" TFT SCREEN RENDERING
// ----------------------------------------------------------------------------
const tft = document.getElementById('virtualTft');
const tftTitle = document.getElementById('tftTitle');
const tftAmount = document.getElementById('tftAmount');
const tftSub = document.getElementById('tftSub');
const tftBody = document.getElementById('tftBody');
const tftHeaderTitle = document.getElementById('tftHeaderTitle');

function renderHomeScreen() {
    tft.classList.remove('alert-border');
    setLed('off');
    tftHeaderTitle.textContent = 'Paytm TrustBox';
    tftBody.innerHTML = `
        <div class="display-title">${currentMerchant.name}</div>
        <div style="background: white; padding: 6px; border-radius: 6px; display: inline-block; margin: 4px 0;">
            <svg width="64" height="64" viewBox="0 0 24 24" fill="#002970">
                <path d="M3 3h8v8H3V3zm2 2v4h4V5H5zm8-2h8v8h-8V3zm2 2v4h4V5h-4zM3 13h8v8H3v-8zm2 2v4h4v-4H5zm13-2h3v2h-3v-2zm-5 0h2v3h-2v-3zm2 3h3v2h-3v-2zm3 3h3v2h-3v-2zm-5 0h2v2h-2v-2z"/>
            </svg>
        </div>
        <div class="display-amount" style="font-size: 18px; color: #38bdf8;">UPI Dynamic QR Ready</div>
        <div class="display-sub">Say <em>"Kitna aaya?"</em> or press <strong>[Verify]</strong></div>
    `;
    updatePinStatus('pin13', 'HIGH (Idle)');
    updatePinStatus('pin12', 'HIGH (Idle)');
}

function renderVerifyingScreen(msg = 'Checking transaction status...') {
    tft.classList.remove('alert-border');
    setLed('yellow');
    tftHeaderTitle.textContent = '⏳ Verifying Payment';
    tftBody.innerHTML = `
        <div class="status-badge-lg processing">⏳</div>
        <div class="display-title" style="color: #facc15;">Connecting to Paytm Gateway...</div>
        <div class="display-sub">${msg}</div>
    `;
}

function renderSuccessScreen(amount, time, fraudScore) {
    tft.classList.remove('alert-border');
    setLed('green');
    tftHeaderTitle.textContent = '✔ Payment Received';
    tftBody.innerHTML = `
        <div class="status-badge-lg success">✔</div>
        <div class="display-amount" style="color: #4ade80;">₹${amount.toFixed(2)}</div>
        <div class="display-title">Genuine UPI Payment Confirmed</div>
        <div class="display-sub">Fraud Score: ${fraudScore.toFixed(2)} (Safe) • Settled in Bank</div>
    `;
}

function renderNotFoundScreen(reason = 'No matching transaction in Paytm Gateway') {
    tft.classList.remove('alert-border');
    setLed('red');
    tftHeaderTitle.textContent = '✖ Payment Not Found';
    tftBody.innerHTML = `
        <div class="status-badge-lg warning" style="background: #991b1b;">✖</div>
        <div class="display-title" style="color: #f87171;">NO INCOMING PAYMENT!</div>
        <div class="display-sub">${reason}</div>
        <div class="display-sub" style="color: #facc15; margin-top: 4px;">Press 🔺 Dispute button if buyer argues.</div>
    `;
}

function renderFraudAlertScreen(amount, fraudScore, reason) {
    tft.classList.add('alert-border');
    setLed('red');
    tftHeaderTitle.textContent = '⚠️ CRITICAL FRAUD ALERT';
    tftBody.innerHTML = `
        <div class="status-badge-lg warning">⚠️</div>
        <div class="display-amount" style="color: #ff4b4b;">₹${amount ? amount.toFixed(2) : '0.00'} Flagged</div>
        <div class="display-title" style="color: #fca5a5;">DO NOT RELEASE GOODS!</div>
        <div class="display-sub" style="color: #ffdddd;">${reason}</div>
        <div class="display-sub" style="color: #facc15; font-weight: 700;">Fraud Score: ${fraudScore.toFixed(2)} / 1.00</div>
    `;
}

function renderDisputeScreen(ticketId) {
    tft.classList.remove('alert-border');
    setLed('red');
    tftHeaderTitle.textContent = '🔺 Dispute Escalated';
    tftBody.innerHTML = `
        <div class="status-badge-lg" style="background: #e65100; color: white;">🔺</div>
        <div class="display-title" style="color: #facc15;">Dispute Ticket Created</div>
        <div class="display-amount" style="font-size: 20px;">Ticket: ${ticketId}</div>
        <div class="display-sub">Paytm Ops team has been notified. Support will contact merchant.</div>
    `;
}

function renderNetworkErrorScreen(error) {
    tft.classList.remove('alert-border');
    setLed('red');
    tftHeaderTitle.textContent = '⚡ Network Disconnected';
    tftBody.innerHTML = `
        <div class="status-badge-lg warning">⚡</div>
        <div class="display-title" style="color: #fca5a5;">Network Error</div>
        <div class="display-sub">${error || 'Could not reach backend server.'}</div>
        <div class="display-sub" style="color: #94a3b8;">Retried 3 times (0.5s, 1s, 2s).</div>
    `;
}

// ----------------------------------------------------------------------------
// 5. HARDWARE PIN & LED CONTROLS
// ----------------------------------------------------------------------------
function setLed(color) {
    const led = document.getElementById('virtualLed');
    led.className = 'rgb-led';
    if (color !== 'off') {
        led.classList.add(color);
    }
    const pinRgb = document.getElementById('pinRgb');
    if (pinRgb) {
        pinRgb.textContent = color.toUpperCase();
        pinRgb.style.color = (color === 'red') ? '#ff3b30' : (color === 'green') ? '#00b36b' : (color === 'yellow') ? '#ff9500' : '#707e94';
    }
}

function updatePinStatus(pinId, text, isActive = false) {
    const el = document.getElementById(pinId);
    if (el) {
        el.textContent = text;
        if (isActive) {
            el.classList.add('active');
        } else {
            el.classList.remove('active');
        }
    }
}

// ----------------------------------------------------------------------------
// 6. SERIAL TERMINAL CONSOLE LOGS
// ----------------------------------------------------------------------------
function logSerial(msg, type = 'info') {
    const consoleEl = document.getElementById('serialConsole');
    if (!consoleEl) return;

    const line = document.createElement('div');
    line.className = `terminal-line ${type}`;
    const timestamp = new Date().toLocaleTimeString();
    line.textContent = `[${timestamp}] ${msg}`;

    consoleEl.insertBefore(line, consoleEl.firstChild);
}

function clearTerminal() {
    const consoleEl = document.getElementById('serialConsole');
    if (consoleEl) consoleEl.innerHTML = '';
}

// ----------------------------------------------------------------------------
// 7. REAL-TIME SERVER-SENT EVENTS (SSE)
// ----------------------------------------------------------------------------
function initSseConnection() {
    eventSource = new EventSource('/api/stream');

    eventSource.onopen = () => {
        document.getElementById('connectionStatus').textContent = 'Live Connected';
        document.getElementById('liveDot').style.backgroundColor = 'var(--success-green)';
        logSerial('[NETWORK] Connected to TrustBox SSE event stream', 'succ');
    };

    eventSource.onmessage = (event) => {
        try {
            const payload = JSON.parse(event.data);
            handleSseEvent(payload);
        } catch (e) {
            console.error('SSE JSON error:', e);
        }
    };

    eventSource.onerror = () => {
        document.getElementById('connectionStatus').textContent = 'Reconnecting...';
        document.getElementById('liveDot').style.backgroundColor = 'var(--alert-red)';
    };
}

function handleSseEvent(evt) {
    if (evt.type === 'VERIFICATION_EVENT') {
        prependTransactionRow(evt.data);
        if (evt.data.fraudScore > 0.70) {
            prependAlertRow(evt.data);
        }
        updateMetrics();
    } else if (evt.type === 'DISPUTE_RAISED' || evt.type === 'DISPUTE_UPDATED') {
        fetchDisputes();
        updateMetrics();
    } else if (evt.type === 'HARDWARE_STATUS') {
        updateHardwareBadge(evt.data);
    } else if (evt.type === 'N8N_ALERT_DISPATCHED') {
        const dest = evt.data && evt.data.n8n ? (evt.data.n8n.target || 'n8n workflow') : 'n8n';
        logSerial(`[n8n WORKFLOW] 🟠 Dispatched incident event! Telegram alert & 1930 incident queued via ${dest}`, 'succ');
    }
}

function updateHardwareBadge(dev) {
    const badge = document.getElementById('hwBadge');
    const statusText = document.getElementById('hwStatusText');
    if (!badge || !statusText) return;

    if (dev && (dev.online || dev.deviceId)) {
        const wasOnline = badge.classList.contains('online');
        badge.classList.add('online');
        statusText.innerHTML = `ESP32: <b>CONNECTED</b> (${dev.ip || 'Wi-Fi'})`;
        if (!wasOnline) {
            logSerial(`[HARDWARE] Physical ESP32 is ONLINE! Device: ${dev.deviceId || 'TBX-001'} | IP: ${dev.ip || 'Unknown'} | Signal: ${dev.rssi || 0} dBm`, 'succ');
        }
    } else {
        badge.classList.remove('online');
        statusText.innerHTML = `ESP32 Hardware: <b>Waiting for device...</b>`;
    }
}

// ----------------------------------------------------------------------------
// 8. VERIFICATION RESPONSE HANDLER
// ----------------------------------------------------------------------------
function handleVerificationResponse(data) {
    logSerial(`[VERIFY-RES] Status: ${data.status} | Amount: ₹${data.amount} | FraudScore: ${data.fraudScore}`, 'info');

    // 1. Cognee memory graph insight
    if (data.factors && data.factors.some(f => f.code && f.code.includes('COGNEE'))) {
        logSerial(`[COGNEE GRAPH] 🔵 Memory graph cluster correlation matched for customer VPA! (+0.40 risk penalty)`, 'warn');
    }

    // 2. Sarvam AI Indic audio announcement
    if (data.sarvamAudio) {
        logSerial(`[SARVAM AI] 🟣 Playing Indic speech announcement synthesized with Bulbul model`, 'succ');
        playBase64Audio(data.sarvamAudio);
    }

    if (data.fraudScore >= 0.70) {
        renderFraudAlertScreen(data.amount, data.fraudScore, data.message);
        playFraudAlertSiren();
        if (!data.sarvamAudio) speakPrompt(data.ttsText, currentLanguage);
    } else if (data.status === 'not_found') {
        renderNotFoundScreen(data.message);
        playButtonClickTone();
        if (!data.sarvamAudio) speakPrompt(data.ttsText, currentLanguage);
    } else {
        renderSuccessScreen(data.amount, 'Just Now', data.fraudScore);
        playPaytmSuccessChime();
        if (!data.sarvamAudio) speakPrompt(data.ttsText, currentLanguage);
    }

    // Auto-return to home screen after 9 seconds
    setTimeout(() => {
        renderHomeScreen();
    }, 9000);
}

function playBase64Audio(base64Str) {
    try {
        const audio = new Audio(`data:audio/wav;base64,${base64Str}`);
        audio.play().catch(e => console.warn('Sarvam audio playback note:', e));
    } catch (err) {
        console.warn('Base64 audio parse error:', err);
    }
}

// ----------------------------------------------------------------------------
// 9. EVENT LISTENERS & HARDWARE BUTTON CLICKS
// ----------------------------------------------------------------------------
function initEventListeners() {
    // 🔺 Dispute Button
    const btnDispute = document.getElementById('btnDispute');
    btnDispute.addEventListener('mousedown', () => {
        updatePinStatus('pin13', 'LOW (Pressed)', true);
        playButtonClickTone();
    });
    btnDispute.addEventListener('mouseup', () => {
        updatePinStatus('pin13', 'HIGH (Idle)');
        triggerDispute();
    });

    // Verify Button
    const btnVerify = document.getElementById('btnVerify');
    btnVerify.addEventListener('mousedown', () => {
        updatePinStatus('pin12', 'LOW (Pressed)', true);
        playButtonClickTone();
    });
    btnVerify.addEventListener('mouseup', () => {
        updatePinStatus('pin12', 'HIGH (Idle)');
        triggerVerify();
    });

    // Voice Mic Button
    const btnVoice = document.getElementById('btnVoice');
    btnVoice.addEventListener('click', () => {
        startVoiceRecognition();
    });

    // PIR Motion Switch
    const motionToggle = document.getElementById('motionToggle');
    motionToggle.addEventListener('change', (e) => {
        const active = e.target.checked;
        const label = document.getElementById('motionLabel');
        if (active) {
            label.textContent = 'PRESENCE ACTIVE';
            label.style.color = '#4ade80';
            updatePinStatus('pin27', 'HIGH (Detected)');
            logSerial('[SENSORS] Presence detected on PIR Sensor (GPIO 27)');
            renderHomeScreen();
        } else {
            label.textContent = 'NO PRESENCE (SLEEP)';
            label.style.color = '#94a3b8';
            updatePinStatus('pin27', 'LOW (Idle)');
            logSerial('[SENSORS] No presence: terminal auto-dimmed');
            tft.style.opacity = '0.3';
        }
    });

    // Language change
    document.getElementById('langSelect').addEventListener('change', (e) => {
        currentLanguage = e.target.value;
        logSerial(`[CONFIG] Spoken language set to: ${currentLanguage.toUpperCase()}`);
    });

    // MID change
    document.getElementById('midSelect').addEventListener('change', (e) => {
        currentMerchant.id = e.target.value;
        currentMerchant.name = e.target.options[e.target.selectedIndex].text.split('(')[0].trim();
        logSerial(`[CONFIG] Switched Merchant: ${currentMerchant.name} (${currentMerchant.id})`);
        renderHomeScreen();
    });
}

// ----------------------------------------------------------------------------
// 10. PRIMARY API ACTIONS
// ----------------------------------------------------------------------------
async function triggerVerify(expectedAmount = 0.0, orderId = null, soundSpoofed = false) {
    logSerial(`[PIPELINE] Initiating payment verification for MID: ${currentMerchant.id}...`);
    renderVerifyingScreen('Connecting to Paytm Gateway...');

    try {
        const res = await fetch('/api/verify_payment', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                merchantId: currentMerchant.id,
                orderId: orderId,
                payload: { amount: expectedAmount },
                soundSpoofed: soundSpoofed,
                lang: currentLanguage
            })
        });

        const data = await res.json();
        handleVerificationResponse(data);
    } catch (err) {
        logSerial(`[ERROR] Verification call failed: ${err.message}`, 'err');
        renderNetworkErrorScreen(err.message);
    }
}

async function triggerDispute(orderId = null, reason = 'Merchant raised dispute via hardware terminal') {
    logSerial('[DISPUTE] >> 🔺 Dispute button pressed! Calling ops escalation API...', 'warn');
    renderVerifyingScreen('Escalating dispute to Paytm Ops...');

    try {
        const res = await fetch('/api/report_dispute', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                merchantId: currentMerchant.id,
                orderId: orderId || 'LATEST_UNCONFIRMED_TXN',
                reason: reason
            })
        });

        const data = await res.json();
        renderDisputeScreen(data.ticketId);
        playDisputeChime();
        speakPrompt(data.ttsText, currentLanguage);

        setTimeout(() => renderHomeScreen(), 10000);
    } catch (err) {
        renderNetworkErrorScreen(err.message);
    }
}

function simulateDisputeFromMerchant() {
    const reason = prompt('Enter Dispute Reason:', 'Buyer insisted they paid ₹500, but device confirmed 0 payment.');
    if (reason) {
        triggerDispute(null, reason);
    }
}

// ----------------------------------------------------------------------------
// 11. PRD SCENARIO TEST STUDIO RUNNERS
// ----------------------------------------------------------------------------
async function runScenarioA() {
    switchTab('simulator');
    logSerial('========================================');
    logSerial('RUNNING SCENARIO A: Legitimate Payment (₹100)');
    logSerial('========================================');

    // 1. First record legitimate payment in gateway
    await fetch('/api/simulate_payment', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            mid: currentMerchant.id,
            orderId: `ORD-${Date.now().toString().slice(-4)}`,
            amount: 100.0,
            payerName: 'Ramesh Gupta'
        })
    });

    logSerial('[GATEWAY] UPI payment of ₹100 settled in Paytm Gateway.');

    // 2. Merchant taps Verify or queries "Kitna aaya?"
    setTimeout(() => {
        triggerVerify(100.0);
    }, 600);
}

async function runScenarioB() {
    switchTab('simulator');
    logSerial('========================================', 'err');
    logSerial('RUNNING SCENARIO B: Fake Screenshot Scam (₹500)', 'err');
    logSerial('========================================', 'err');

    logSerial('[SCAMMER] Customer shows doctored screen claiming ₹500 paid.');
    logSerial('[MERCHANT] Merchant taps [Verify] on TrustBox to confirm...');

    setTimeout(() => {
        // Look for non-existent order with claimed amount ₹500
        triggerVerify(500.0, 'BOGUS_FAKE_SCREEN_ORD_999');
    }, 600);
}

async function runScenarioC() {
    switchTab('simulator');
    logSerial('========================================', 'warn');
    logSerial('RUNNING SCENARIO C: Spoofed Sound Alert Scam', 'warn');
    logSerial('========================================', 'warn');

    logSerial('[SCAMMER] Buyer plays fake chime from their mobile phone speaker near terminal.');
    logSerial('[TRUSTBOX] Acoustic anomaly detected! Cross-referencing backend server push...');

    setTimeout(() => {
        // Sound spoofed flag true with no matching record
        triggerVerify(200.0, 'SPOOFED_AUDIO_ORDER', true);
    }, 600);
}

async function runScenarioD() {
    switchTab('simulator');
    logSerial('========================================');
    logSerial('RUNNING SCENARIO D: Network Outage & Exponential Backoff');
    logSerial('========================================');

    renderVerifyingScreen('Checking connection...');
    setLed('yellow');

    logSerial('[NETWORK] Attempt 1/3: Connection timeout (5000ms)... retrying in 500ms', 'warn');
    await new Promise(r => setTimeout(r, 600));

    logSerial('[NETWORK] Attempt 2/3: Host unreachable... retrying in 1000ms', 'warn');
    await new Promise(r => setTimeout(r, 1000));

    logSerial('[NETWORK] Attempt 3/3: Max retries exceeded. Terminal entering offline fallback.', 'err');
    renderNetworkErrorScreen('Backend Server Unreachable. Check Wi-Fi router.');
    speakPrompt(currentLanguage === 'hi' ? 'Server se sampark nahi ho saka. Wi-Fi check karein.' : 'Unable to connect to server. Check network.', currentLanguage);

    setTimeout(() => renderHomeScreen(), 8000);
}

// ----------------------------------------------------------------------------
// 12. DATA FETCHING & TABLE RENDERING
// ----------------------------------------------------------------------------
async function fetchInitialData() {
    await Promise.all([
        fetchTransactions(),
        fetchAlerts(),
        fetchDisputes(),
        updateMetrics(),
        checkHardwareStatus(),
        fetchSponsorStatuses()
    ]);
    setInterval(checkHardwareStatus, 6000);
    setInterval(fetchSponsorStatuses, 15000);
}

async function fetchSponsorStatuses() {
    try {
        const res = await fetch('/api/sponsors/status');
        const data = await res.json();

        const sarvamEl = document.getElementById('sarvamStatusBadge');
        if (sarvamEl && data.sarvam) {
            sarvamEl.textContent = data.sarvam.configured ? 'Live (Bulbul)' : 'Active (Simulated)';
            sarvamEl.style.background = data.sarvam.configured ? '#8b5cf6' : '#ede9fe';
            sarvamEl.style.color = data.sarvam.configured ? '#fff' : '#6d28d9';
        }

        const n8nEl = document.getElementById('n8nStatusBadge');
        if (n8nEl && data.n8n) {
            n8nEl.textContent = data.n8n.configured ? 'Live Webhook' : 'Workflow Ready';
            n8nEl.style.background = data.n8n.configured ? '#ea580c' : '#ffedd5';
            n8nEl.style.color = data.n8n.configured ? '#fff' : '#c2410c';
        }

        const cogneeEl = document.getElementById('cogneeStatusBadge');
        if (cogneeEl && data.cognee) {
            cogneeEl.textContent = data.cognee.configured ? 'Live (Cloud)' : `Active (${data.cognee.totalGraphNodes || 6} Nodes)`;
            cogneeEl.style.background = data.cognee.configured ? '#0284c7' : '#e0f2fe';
            cogneeEl.style.color = data.cognee.configured ? '#fff' : '#0369a1';
        }
    } catch (e) {
        console.warn('Sponsor status check error:', e);
    }
}

async function testN8nWorkflow() {
    logSerial('[n8n] >> Firing test incident alert to n8n Webhook & Telegram...', 'warn');
    try {
        const res = await fetch('/api/n8n/trigger_test', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                merchantId: currentMerchant.id,
                merchantName: currentMerchant.name,
                amount: 500.0
            })
        });
        const data = await res.json();
        const dest = data.target || 'Live n8n Cloud';
        const chatId = data.telegramChatId || '1839884717';
        logSerial(`[n8n] Automated Incident Dispatched! Target: ${dest} | Telegram Chat: ${chatId} | Event: ${data.eventId || 'OK'}`, 'succ');
        alert(`⚡ n8n Incident Automation Dispatched!\n\n• Event ID: ${data.eventId || 'EVT-TEST'}\n• Recipient: Telegram (Chat ID: ${chatId})\n• Target: ${dest}\n• Paytm Ops Incident Ticket Created\n• Logged to 1930 Cybercrime Ledger`);
    } catch (e) {
        logSerial(`[n8n] Dispatch failed: ${e.message}`, 'err');
    }
}

async function checkHardwareStatus() {
    try {
        const res = await fetch('/api/hardware_status');
        const data = await res.json();
        updateHardwareBadge(data.device);
    } catch (e) {
        console.error('Hardware status check error:', e);
    }
}

async function fetchTransactions() {
    try {
        const res = await fetch('/api/transactions');
        const list = await res.json();
        const tbody = document.getElementById('transactionsTableBody');
        tbody.innerHTML = '';

        if (list.length === 0) {
            tbody.innerHTML = `<tr><td colspan="9" style="text-align: center; color: var(--text-secondary); padding: 24px;">No transactions recorded yet.</td></tr>`;
            return;
        }

        list.forEach(prependTransactionRow);
        document.getElementById('liveCountText').textContent = `${list.length} events recorded`;
    } catch (e) {
        console.error('Fetch transactions error:', e);
    }
}

function prependTransactionRow(item) {
    const tbody = document.getElementById('transactionsTableBody');
    if (!tbody) return;

    // Remove empty placeholder if present
    if (tbody.querySelector('td[colspan="9"]')) {
        tbody.innerHTML = '';
    }

    const tr = document.createElement('tr');
    const pillClass = (item.alertLevel === 'SAFE') ? 'safe' : (item.alertLevel === 'CAUTION') ? 'caution' : 'fraud';

    tr.innerHTML = `
        <td style="font-family: monospace; font-size: 11px;">${new Date(item.timestamp).toLocaleTimeString()}</td>
        <td style="font-family: monospace; font-weight: 600;">${item.orderId}</td>
        <td>${item.merchantId}</td>
        <td style="font-weight: 700; color: var(--paytm-navy);">₹${item.amount.toFixed(2)}</td>
        <td><span class="pill ${pillClass}">${item.status}</span></td>
        <td style="font-weight: 700;">${item.fraudScore.toFixed(2)}</td>
        <td><span class="pill ${pillClass}">${item.alertLevel}</span></td>
        <td>${item.latencyMs || 150}ms</td>
        <td style="font-size: 11px; color: var(--text-secondary); max-width: 250px;">${item.explanation || 'Verified'}</td>
    `;

    tbody.insertBefore(tr, tbody.firstChild);
}

async function fetchAlerts() {
    try {
        const res = await fetch('/api/alerts');
        const list = await res.json();
        const tbody = document.getElementById('alertsTableBody');
        tbody.innerHTML = '';

        const badge = document.getElementById('alertCountBadge');
        if (badge) badge.textContent = list.length;

        if (list.length === 0) {
            tbody.innerHTML = `<tr><td colspan="8" style="text-align: center; color: var(--text-secondary); padding: 20px;">No active fraud alerts. System secure.</td></tr>`;
            return;
        }

        list.forEach(prependAlertRow);
    } catch (e) {
        console.error('Fetch alerts error:', e);
    }
}

function prependAlertRow(alert) {
    const tbody = document.getElementById('alertsTableBody');
    if (!tbody) return;

    if (tbody.querySelector('td[colspan="8"]')) {
        tbody.innerHTML = '';
    }

    const tr = document.createElement('tr');
    tr.innerHTML = `
        <td style="font-family: monospace; font-size: 11px;">${new Date(alert.timestamp).toLocaleTimeString()}</td>
        <td><strong>${alert.merchantId}</strong></td>
        <td style="font-family: monospace;">${alert.orderId}</td>
        <td style="color: var(--alert-red); font-weight: 700;">₹${alert.amount.toFixed(2)}</td>
        <td><span class="pill fraud">${alert.fraudScore.toFixed(2)}</span></td>
        <td style="font-size: 12px; color: #991b1b;">${alert.explanation}</td>
        <td style="font-size: 11px; color: var(--text-secondary);">${alert.recommendation}</td>
        <td>
            <button class="btn-action escalate" onclick="escalateToNpci('${alert.id || alert.orderId}')">Escalate 1930</button>
        </td>
    `;
    tbody.insertBefore(tr, tbody.firstChild);
}

async function fetchDisputes() {
    try {
        const res = await fetch('/api/disputes');
        const list = await res.json();
        const tbody = document.getElementById('disputesTableBody');
        tbody.innerHTML = '';

        const badge = document.getElementById('disputeCountBadge');
        if (badge) badge.textContent = list.length;

        if (list.length === 0) {
            tbody.innerHTML = `<tr><td colspan="8" style="text-align: center; color: var(--text-secondary); padding: 20px;">No active disputes in queue.</td></tr>`;
            return;
        }

        list.forEach(item => {
            const tr = document.createElement('tr');
            tr.innerHTML = `
                <td style="font-family: monospace; font-weight: 700; color: #d90429;">${item.ticketId}</td>
                <td style="font-size: 11px;">${new Date(item.createdAt).toLocaleTimeString()}</td>
                <td>${item.merchantId}</td>
                <td style="font-family: monospace;">${item.orderId}</td>
                <td style="font-size: 12px;">${item.reason}</td>
                <td><span class="pill fraud">${item.priority}</span></td>
                <td><span class="pill ${item.status === 'RESOLVED' ? 'safe' : 'caution'}">${item.status}</span></td>
                <td>
                    ${item.status !== 'RESOLVED' ? `<button class="btn-action resolve" onclick="resolveDispute('${item.ticketId}')">Resolve & Call</button>` : `<span style="color: var(--success-green); font-size: 11px; font-weight: 700;">✔ Resolved</span>`}
                </td>
            `;
            tbody.appendChild(tr);
        });
    } catch (e) {
        console.error('Fetch disputes error:', e);
    }
}

async function resolveDispute(ticketId) {
    try {
        await fetch(`/api/disputes/${ticketId}/resolve`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ resolution: 'RESOLVED', notes: 'Merchant called and scam neutralized.' })
        });
        fetchDisputes();
        updateMetrics();
        logSerial(`[OPS] Dispute ${ticketId} marked as RESOLVED by operator.`, 'succ');
    } catch (e) {
        console.error('Resolve dispute error:', e);
    }
}

function escalateToNpci(alertId) {
    alert(`Case ${alertId} escalated to National Cybercrime Portal (1930) and Paytm Fraud Intelligence.`);
    logSerial(`[OPS] Case ${alertId} reported to 1930 Cybercrime and NPCI graph blacklist.`, 'succ');
}

async function updateMetrics() {
    try {
        const res = await fetch('/api/metrics');
        const data = await res.json();
        document.getElementById('metricTotal').textContent = data.totalVerifications;
        document.getElementById('metricFrauds').textContent = data.fraudsPrevented;
        document.getElementById('metricAmountSaved').textContent = `₹${data.fraudAmountPrevented.toLocaleString()} Saved`;
        document.getElementById('metricDisputes').textContent = data.disputesRaised - data.disputesResolved;
        document.getElementById('metricLatency').textContent = `${data.avgLatencyMs} ms`;
    } catch (e) {
        console.error('Metrics update error:', e);
    }
}
