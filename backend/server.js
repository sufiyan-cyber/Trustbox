/**
 * ============================================================================
 * PAYTM TRUSTBOX: BACKEND SERVER & REAL-TIME API
 * ============================================================================
 * Primary backend microservice handling ESP32 terminal verification,
 * voice query parsing, Paytm UPI API integration, ML fraud scoring,
 * and live operations dashboard updates via Server-Sent Events (SSE).
 */

if (process.loadEnvFile) {
    try { process.loadEnvFile(); } catch (e) {}
}

const express = require('express');
const cors = require('cors');
const path = require('path');
const PaytmClient = require('./paytm_client');
const FraudEngine = require('./fraud_engine');
const VoiceService = require('./voice_service');

const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Serve Dashboard & Hardware Simulator UI
app.use(express.static(path.join(__dirname, '../dashboard')));

// Core Domain Services
const SarvamService = require('./sarvam_service');
const N8nService = require('./n8n_service');
const CogneeService = require('./cognee_service');

const paytmGateway = new PaytmClient();
const sarvamService = new SarvamService();
const n8nService = new N8nService();
const cogneeService = new CogneeService();
const fraudEngine = new FraudEngine({ cogneeService });
const voiceService = new VoiceService();

// State Storage
const verificationLogs = [];
const disputeQueue = [];
const sseClients = new Set();

let metrics = {
    totalVerifications: 14,
    fraudsPrevented: 3,
    fraudAmountPrevented: 8250.00,
    disputesRaised: 2,
    disputesResolved: 1,
    avgLatencyMs: 180,
    subscribedMerchants: 1280,
    subscriptionTier: '₹149/mo (Premium AI Anti-Fraud)'
};

/**
 * Server-Sent Events (SSE) Stream for real-time dashboard updates
 */
app.get('/api/stream', (req, res) => {
    res.setHeader('Content-Type', 'text/event-stream');
    res.setHeader('Cache-Control', 'no-cache');
    res.setHeader('Connection', 'keep-alive');
    res.flushHeaders();

    sseClients.add(res);

    // Initial keepalive ping
    res.write(`data: ${JSON.stringify({ type: 'CONNECTED', timestamp: new Date().toISOString() })}\n\n`);

    req.on('close', () => {
        sseClients.delete(res);
    });
});

function broadcastEvent(eventType, payload) {
    const data = JSON.stringify({ type: eventType, data: payload, timestamp: new Date().toISOString() });
    for (const client of sseClients) {
        try {
            client.write(`data: ${data}\n\n`);
        } catch (err) {
            sseClients.delete(client);
        }
    }
}

let lastHardwarePing = null;
let hardwareDeviceInfo = null;

// Hardware Ping / Heartbeat from ESP32
app.post(['/api/heartbeat', '/api/ping'], (req, res) => {
    lastHardwarePing = Date.now();
    const rawIp = req.headers['x-forwarded-for'] || req.socket.remoteAddress || '127.0.0.1';
    const cleanIp = rawIp.replace('::ffff:', '');
    
    hardwareDeviceInfo = {
        deviceId: req.body.deviceId || 'TBX-ESP32-001',
        ip: req.body.ip || cleanIp,
        rssi: req.body.rssi || -55,
        firmware: req.body.firmware || 'v1.0.0-PROD',
        merchantId: req.body.merchantId || 'M12345678',
        timestamp: new Date().toISOString()
    };

    broadcastEvent('HARDWARE_STATUS', { online: true, ...hardwareDeviceInfo });
    return res.status(200).json({ status: 'connected', serverTime: new Date().toISOString() });
});

app.get('/api/hardware_status', (req, res) => {
    const isOnline = Boolean(lastHardwarePing && (Date.now() - lastHardwarePing < 25000));
    return res.json({
        online: isOnline,
        lastSeenSecondsAgo: lastHardwarePing ? Math.round((Date.now() - lastHardwarePing) / 1000) : null,
        device: isOnline ? hardwareDeviceInfo : null
    });
});

// ----------------------------------------------------------------------------
// 1. ENDPOINT: VERIFY PAYMENT (Sense -> Connect -> Investigate -> Predict -> Act)
// Supports both /api/verify_payment and /api/verify aliases
// ----------------------------------------------------------------------------
app.post(['/api/verify_payment', '/api/verify'], async (req, res) => {
    // If device sends action="dispute" to verify endpoint, route to dispute
    if (req.body && req.body.action === 'dispute') {
        const merchantId = req.body.merchantId || 'M12345678';
        const orderId = req.body.orderId || req.body.transactionId;
        const reason = req.body.reason || 'Merchant reported fraud / dispute via device';
        return handleCreateDispute(merchantId, orderId, reason, res);
    }

    const startTime = Date.now();
    const {
        merchantId = 'M12345678',
        orderId = req.body.transactionId,
        payload = {},
        soundSpoofed = false,
        customerUpi,
        lang = 'hi'
    } = req.body;

    const targetOrderId = orderId || req.body.transactionId;
    const expectedAmount = payload.amount ? parseFloat(payload.amount) : (req.body.amount ? parseFloat(req.body.amount) : null);

    // Step 1: Query Paytm Gateway
    const gatewayResult = await paytmGateway.checkTransactionStatus(merchantId, targetOrderId);

    // Step 2: Evaluate Fraud Risk
    const riskAssessment = fraudEngine.evaluateRisk({
        merchantId,
        orderId,
        expectedAmount,
        gatewayResult,
        soundSpoofed,
        customerUpi,
        claimedTimestamp: req.body.timestamp
    });

    const latencyMs = Date.now() - startTime;
    metrics.totalVerifications++;
    if (riskAssessment.isFlagged) {
        metrics.fraudsPrevented++;
        metrics.fraudAmountPrevented += (expectedAmount || (gatewayResult.found ? gatewayResult.amount : 500));
    }

    let status = 'confirmed';
    let promptType = 'success';
    let amount = gatewayResult.found ? gatewayResult.amount : (expectedAmount || 0);

    if (riskAssessment.fraudScore >= 0.70) {
        status = 'flagged_fraud';
        promptType = 'fraud_alert';
    } else if (!gatewayResult.found) {
        status = 'not_found';
        promptType = 'not_found';
    }

    const voicePrompt = voiceService.getSpeechPrompt(promptType, {
        amount,
        reason: riskAssessment.explanation
    }, lang);

    const record = {
        id: `LOG-${Date.now().toString().slice(-6)}`,
        merchantId,
        orderId: gatewayResult.orderId || orderId || 'UNRECORDED',
        amount,
        status,
        fraudScore: riskAssessment.fraudScore,
        alertLevel: riskAssessment.riskTier,
        factors: riskAssessment.factors,
        explanation: riskAssessment.explanation,
        recommendation: riskAssessment.recommendation,
        ttsText: voicePrompt.text,
        alertTone: voicePrompt.alertTone,
        timestamp: new Date().toISOString(),
        latencyMs
    };

    // 1. Ingest incident into Cognee memory graph if high risk
    if (riskAssessment.fraudScore >= 0.70) {
        cogneeService.ingestIncident({
            customerUpi: customerUpi || (gatewayResult ? gatewayResult.payerUpi : null),
            merchantId,
            orderId: record.orderId,
            fraudScore: record.fraudScore,
            reason: record.explanation
        });

        // 2. Trigger automated n8n incident workflow (WhatsApp alert & 1930 Cybercrime)
        n8nService.triggerFraudAlert(record).then(n8nRes => {
            broadcastEvent('N8N_ALERT_DISPATCHED', { alert: record, n8n: n8nRes });
        }).catch(err => console.warn('[n8n] Trigger error:', err.message));
    }

    // 3. Generate Indic voice announcement via Sarvam AI
    const sarvamSpeech = await sarvamService.generateSpeech({
        text: voicePrompt.text,
        languageCode: (lang === 'hi' ? 'hi-IN' : 'en-IN'),
        speaker: 'meera'
    });

    if (sarvamSpeech && sarvamSpeech.audioBase64) {
        record.sarvamAudio = sarvamSpeech.audioBase64;
    }

    verificationLogs.unshift(record);
    if (verificationLogs.length > 100) verificationLogs.pop();

    // Push live event to Ops Dashboard & Simulator
    broadcastEvent('VERIFICATION_EVENT', record);

    return res.status(200).json({
        status,
        amount,
        orderId: record.orderId,
        fraudScore: riskAssessment.fraudScore,
        alertLevel: riskAssessment.riskTier,
        message: riskAssessment.explanation,
        ttsText: voicePrompt.text,
        sarvamAudio: record.sarvamAudio || null,
        alertTone: voicePrompt.alertTone,
        factors: riskAssessment.factors,
        recommendation: riskAssessment.recommendation,
        timestamp: record.timestamp,
        latencyMs,
        sponsors: {
            sarvam: { active: true, model: 'bulbul:v1' },
            n8n: { triggered: riskAssessment.fraudScore >= 0.70 },
            cognee: { graphAnalyzed: true }
        }
    });
});

// ----------------------------------------------------------------------------
// 2. ENDPOINT: VOICE QUERY ("Kitna aaya?")
// ----------------------------------------------------------------------------
app.post('/api/voice_query', async (req, res) => {
    const { merchantId = 'M12345678', query = 'Kitna aaya?', lang = 'hi' } = req.body;

    const parsed = voiceService.parseVoiceQuery(query);

    if (parsed.intent === 'RAISE_DISPUTE') {
        return handleCreateDispute(merchantId, 'VOICE_TRIGGERED', `Merchant voice dispute: "${query}"`, res);
    }

    // Otherwise verify latest payment
    const gatewayResult = await paytmGateway.checkTransactionStatus(merchantId, 'LATEST');
    const riskAssessment = fraudEngine.evaluateRisk({
        merchantId,
        orderId: gatewayResult.orderId,
        expectedAmount: parsed.amount,
        gatewayResult
    });

    let status = gatewayResult.found ? 'confirmed' : 'not_found';
    let promptType = gatewayResult.found ? 'success' : 'not_found';
    if (riskAssessment.fraudScore >= 0.70) {
        status = 'flagged_fraud';
        promptType = 'fraud_alert';
    }

    const voicePrompt = voiceService.getSpeechPrompt(promptType, {
        amount: gatewayResult.found ? gatewayResult.amount : 0,
        reason: riskAssessment.explanation
    }, lang);

    const responsePayload = {
        status,
        query: parsed.rawQuery,
        intent: parsed.intent,
        amount: gatewayResult.found ? gatewayResult.amount : 0,
        orderId: gatewayResult.orderId || 'NONE',
        fraudScore: riskAssessment.fraudScore,
        alertLevel: riskAssessment.riskTier,
        message: riskAssessment.explanation,
        ttsText: voicePrompt.text,
        alertTone: voicePrompt.alertTone,
        timestamp: new Date().toISOString()
    };

    broadcastEvent('VOICE_QUERY_EVENT', responsePayload);
    return res.status(200).json(responsePayload);
});

// ----------------------------------------------------------------------------
// 3. ENDPOINT: REPORT DISPUTE (🔺 Button pressed)
// ----------------------------------------------------------------------------
function handleCreateDispute(merchantId, orderId, reason, res) {
    metrics.disputesRaised++;
    const ticketId = `DSP-${Date.now().toString().slice(-4)}`;

    const dispute = {
        ticketId,
        merchantId,
        orderId: orderId || 'LATEST_SUSPECT_TXN',
        reason: reason || 'Merchant raised dispute via hardware terminal (🔺 Button)',
        status: 'OPEN',
        priority: 'HIGH',
        createdAt: new Date().toISOString(),
        assignedAgent: 'Paytm Risk & Ops Escalation Cell',
        notes: []
    };

    disputeQueue.unshift(dispute);
    broadcastEvent('DISPUTE_RAISED', dispute);

    // Trigger n8n Dispute Escalation Workflow
    n8nService.triggerDispute(dispute).catch(err => console.warn('[n8n] Dispute trigger error:', err.message));

    // Ingest dispute into Cognee memory graph
    cogneeService.ingestIncident({
        customerUpi: 'disputed_buyer',
        merchantId,
        orderId: dispute.orderId,
        fraudScore: 0.90,
        reason: dispute.reason
    });

    return res.status(201).json({
        status: 'dispute_registered',
        ticketId,
        message: 'Dispute registered with Paytm Ops. Support agent alerted.',
        ttsText: `Dispute registered. Ticket number ${ticketId}. Operations team will call you.`
    });
}

app.post(['/api/report_dispute', '/api/dispute'], (req, res) => {
    const merchantId = req.body.merchantId || 'M12345678';
    const orderId = req.body.orderId || req.body.transactionId;
    const reason = req.body.reason;
    return handleCreateDispute(merchantId, orderId, reason, res);
});

// ----------------------------------------------------------------------------
// 4. ENDPOINT: RESOLVE DISPUTE
// ----------------------------------------------------------------------------
app.post('/api/disputes/:id/resolve', (req, res) => {
    const { id } = req.params;
    const { resolution = 'RESOLVED', notes = 'Investigated by ops' } = req.body;

    const dispute = disputeQueue.find(d => d.ticketId === id);
    if (!dispute) {
        return res.status(404).json({ error: 'Dispute ticket not found' });
    }

    dispute.status = resolution;
    dispute.resolvedAt = new Date().toISOString();
    dispute.notes.push({ text: notes, time: new Date().toISOString() });
    metrics.disputesResolved++;

    broadcastEvent('DISPUTE_UPDATED', dispute);
    return res.status(200).json(dispute);
});

// ----------------------------------------------------------------------------
// 5. ENDPOINT: SIMULATE INCOMING PAYMENT (For Demo Scenarios)
// ----------------------------------------------------------------------------
app.post('/api/simulate_payment', (req, res) => {
    const {
        mid = 'M12345678',
        orderId,
        amount = 100.0,
        payerName = 'Test Customer',
        payerUpi = 'customer@upi'
    } = req.body;

    const record = paytmGateway.recordPayment({
        mid,
        orderId,
        amount,
        payerName,
        payerUpi
    });

    broadcastEvent('PAYMENT_RECEIVED', record);
    return res.status(201).json({
        message: 'Payment received in Paytm Gateway ledger',
        record
    });
});

// ----------------------------------------------------------------------------
// 6. ENDPOINT: PAYTM S2S WEBHOOK (Real Paytm / Sandbox Payment Callback)
// ----------------------------------------------------------------------------
app.post(['/api/paytm_webhook', '/api/paytm_callback'], (req, res) => {
    console.log('[PAYTM WEBHOOK] Incoming payment notification from Paytm:', req.body);
    const record = paytmGateway.handlePaytmWebhook(req.body);
    broadcastEvent('PAYMENT_RECEIVED', record);
    return res.status(200).send('SUCCESS');
});

// ----------------------------------------------------------------------------
// 6. QUERY ENDPOINTS FOR DASHBOARD
// ----------------------------------------------------------------------------
app.get('/api/transactions', (req, res) => {
    const { mid } = req.query;
    res.json(verificationLogs);
});

app.get('/api/alerts', (req, res) => {
    const highRiskAlerts = verificationLogs.filter(l => l.fraudScore > 0.70);
    res.json(highRiskAlerts);
});

app.get('/api/disputes', (req, res) => {
    res.json(disputeQueue);
});

app.get('/api/metrics', (req, res) => {
    res.json(metrics);
});

// ----------------------------------------------------------------------------
// 7. SPONSOR AI INTEGRATIONS API (Sarvam AI, n8n, Cognee)
// ----------------------------------------------------------------------------
app.get('/api/sponsors/status', (req, res) => {
    res.json({
        sarvam: sarvamService.getStatus(),
        n8n: n8nService.getStatus(),
        cognee: cogneeService.getStatus()
    });
});

app.post('/api/sarvam/tts', async (req, res) => {
    const { text = 'Paytm TrustBox payment verified', languageCode = 'hi-IN', speaker = 'meera' } = req.body;
    const result = await sarvamService.generateSpeech({ text, languageCode, speaker });
    res.json(result);
});

app.post('/api/sarvam/stt', async (req, res) => {
    const { languageCode = 'hi-IN' } = req.body;
    const result = await sarvamService.transcribeAudio({ languageCode });
    res.json(result);
});

app.get('/api/cognee/graph', (req, res) => {
    res.json({
        status: 'OK',
        nodes: Array.from(cogneeService.graphNodes.entries()).map(([k, v]) => ({ id: k, ...v })),
        edges: cogneeService.graphEdges,
        summary: cogneeService.getStatus()
    });
});

app.post('/api/n8n/trigger_test', async (req, res) => {
    const testAlert = {
        merchantId: req.body.merchantId || 'M12345678',
        merchantName: req.body.merchantName || 'Rajesh Kirana Store',
        orderId: 'TEST-ORD-' + Date.now().toString().slice(-4),
        amount: req.body.amount || 500.0,
        fraudScore: 1.0,
        alertLevel: 'CRITICAL_FRAUD',
        explanation: 'Manual test of n8n Telegram alert & cybercrime escalation',
        recommendation: 'DO NOT hand over goods!'
    };
    const result = await n8nService.triggerFraudAlert(testAlert);
    broadcastEvent('N8N_ALERT_DISPATCHED', { alert: testAlert, n8n: result });
    res.json({
        ...result,
        telegramChatId: n8nService.telegramChatId || '1839884717',
        target: n8nService.webhookUrl || 'n8n Cloud Webhook'
    });
});


// Start Server
app.listen(PORT, () => {
    console.log(`====================================================`);
    console.log(` PAYTM TRUSTBOX BACKEND SERVER ACTIVE`);
    console.log(` URL: http://localhost:${PORT}`);
    console.log(` Ops Portal & Simulator: http://localhost:${PORT}/index.html`);
    console.log(`====================================================`);
});

module.exports = app;
