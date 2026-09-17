/**
 * ============================================================================
 * PAYTM TRUSTBOX: AUTOMATED TEST SUITE
 * ============================================================================
 * Verifies all 4 PRD Test Scenarios, Fraud Scoring Heuristics,
 * Voice Query Parsing, and Dispute Escalation Workflows.
 */

const assert = require('assert');
const PaytmClient = require('./paytm_client');
const FraudEngine = require('./fraud_engine');
const VoiceService = require('./voice_service');

console.log('====================================================');
console.log(' RUNNING PAYTM TRUSTBOX AUTOMATED TEST SUITE');
console.log('====================================================\n');

let passedTests = 0;
let totalTests = 0;

function runTest(testName, fn) {
    totalTests++;
    try {
        fn();
        console.log(`  [PASS] ${testName}`);
        passedTests++;
    } catch (err) {
        console.error(`  [FAIL] ${testName}`);
        console.error(`         Error: ${err.message}`);
    }
}

async function runAsyncTest(testName, fn) {
    totalTests++;
    try {
        await fn();
        console.log(`  [PASS] ${testName}`);
        passedTests++;
    } catch (err) {
        console.error(`  [FAIL] ${testName}`);
        console.error(`         Error: ${err.message}`);
    }
}

async function executeAll() {
    const paytm = new PaytmClient();
    const fraudEngine = new FraudEngine();
    const voice = new VoiceService();

    // -------------------------------------------------------------
    // 1. GATEWAY ADAPTER TESTS
    // -------------------------------------------------------------
    console.log('--- 1. Paytm Gateway & Checksum Verification ---');

    runTest('Checksum generation and verification consistency', () => {
        const params = { mid: 'M12345678', orderId: 'ORD-1001', amount: 250 };
        const checksum = paytm.generateChecksum(params);
        assert.ok(checksum && checksum.length === 64, 'Checksum must be 64-char SHA256 hex');
        assert.strictEqual(paytm.verifyChecksum(params, checksum), true, 'Checksum verification failed');
    });

    await runAsyncTest('Query existing genuine transaction (ORD-1001)', async () => {
        const res = await paytm.checkTransactionStatus('M12345678', 'ORD-1001');
        assert.strictEqual(res.found, true);
        assert.strictEqual(res.status, 'TXN_SUCCESS');
        assert.strictEqual(res.amount, 250.0);
    });

    await runAsyncTest('Query non-existent transaction returns not found', async () => {
        const res = await paytm.checkTransactionStatus('M12345678', 'NON_EXISTENT_ORDER');
        assert.strictEqual(res.found, false);
        assert.strictEqual(res.reason, 'NO_RECORD_FOUND_IN_PAYTM_GATEWAY');
    });

    // -------------------------------------------------------------
    // 2. PRD TEST SCENARIOS
    // -------------------------------------------------------------
    console.log('\n--- 2. PRD Test Scenarios ---');

    // SCENARIO A: Legitimate Payment
    await runAsyncTest('Scenario A: Legitimate Payment -> Confirmed, FraudScore < 0.3', async () => {
        // Record legitimate ₹100 payment
        paytm.recordPayment({
            mid: 'M12345678',
            orderId: 'ORD-LEGIT-100',
            amount: 100.0,
            payerName: 'Ramesh Gupta'
        });

        const gatewayRes = await paytm.checkTransactionStatus('M12345678', 'ORD-LEGIT-100');
        const assessment = fraudEngine.evaluateRisk({
            merchantId: 'M12345678',
            orderId: 'ORD-LEGIT-100',
            expectedAmount: 100.0,
            gatewayResult: gatewayRes
        });

        assert.strictEqual(gatewayRes.found, true);
        assert.strictEqual(assessment.riskTier, 'SAFE');
        assert.ok(assessment.fraudScore < 0.30, `Score ${assessment.fraudScore} must be < 0.30`);
        assert.strictEqual(assessment.isFlagged, false);
    });

    // SCENARIO B: Fake Screenshot Scam
    await runAsyncTest('Scenario B: Fake Payment Screenshot -> No Gateway Record -> FraudScore 1.0 (Flagged)', async () => {
        // Scammer shows screen for ₹500, but no payment was made
        const gatewayRes = await paytm.checkTransactionStatus('M12345678', 'FAKE_SCREENSHOT_ORD');
        const assessment = fraudEngine.evaluateRisk({
            merchantId: 'M12345678',
            orderId: 'FAKE_SCREENSHOT_ORD',
            expectedAmount: 500.0,
            gatewayResult: gatewayRes
        });

        assert.strictEqual(gatewayRes.found, false);
        assert.strictEqual(assessment.fraudScore, 1.0, 'Missing record must yield maximum fraud score 1.0');
        assert.strictEqual(assessment.riskTier, 'FRAUD');
        assert.strictEqual(assessment.isFlagged, true);
        assert.ok(assessment.factors.some(f => f.code === 'NO_GATEWAY_RECORD'));
    });

    // SCENARIO C: Sound Spoofing (Customer played fake chime app)
    await runAsyncTest('Scenario C: Sound Spoofing -> Acoustic anomaly flagged without gateway match', async () => {
        const gatewayRes = await paytm.checkTransactionStatus('M12345678', 'UNVERIFIED_SOUND_ORD');
        const assessment = fraudEngine.evaluateRisk({
            merchantId: 'M12345678',
            orderId: 'UNVERIFIED_SOUND_ORD',
            expectedAmount: 200.0,
            gatewayResult: gatewayRes,
            soundSpoofed: true
        });

        assert.ok(assessment.fraudScore >= 0.95, `Audio spoof score ${assessment.fraudScore} must be >= 0.95`);
        assert.strictEqual(assessment.riskTier, 'FRAUD');
        assert.strictEqual(assessment.isFlagged, true);
    });

    // RULE 2 TEST: Amount Discrepancy (Customer paid ₹10, showed screenshot of ₹1000)
    await runAsyncTest('Rule 2: Amount Discrepancy Scam -> FraudScore 0.85', async () => {
        paytm.recordPayment({
            mid: 'M12345678',
            orderId: 'ORD-MISMATCH-10',
            amount: 10.0
        });

        const gatewayRes = await paytm.checkTransactionStatus('M12345678', 'ORD-MISMATCH-10');
        const assessment = fraudEngine.evaluateRisk({
            merchantId: 'M12345678',
            orderId: 'ORD-MISMATCH-10',
            expectedAmount: 1000.0, // Scammer claims ₹1000
            gatewayResult: gatewayRes
        });

        assert.ok(assessment.fraudScore >= 0.85, `Discrepancy score ${assessment.fraudScore} must be >= 0.85`);
        assert.strictEqual(assessment.riskTier, 'FRAUD');
        assert.ok(assessment.factors.some(f => f.code === 'AMOUNT_MISMATCH'));
    });

    // -------------------------------------------------------------
    // 3. VOICE INTENT & SPEECH GENERATION TESTS
    // -------------------------------------------------------------
    console.log('\n--- 3. Multilingual Voice & Speech Services ---');

    runTest('Voice Query Intent Parsing ("Kitna aaya?")', () => {
        const parsed = voice.parseVoiceQuery('Kitna aaya?');
        assert.strictEqual(parsed.intent, 'QUERY_LATEST_PAYMENT');
    });

    runTest('Voice Query with Amount ("500 aaya kya?")', () => {
        const parsed = voice.parseVoiceQuery('500 rupaye aaya kya?');
        assert.strictEqual(parsed.amount, 500);
    });

    runTest('Voice Dispute Intent ("Dispute raise karo / Fraud ho gaya")', () => {
        const parsed = voice.parseVoiceQuery('Yeh fraud lag raha hai dispute raise karo');
        assert.strictEqual(parsed.intent, 'RAISE_DISPUTE');
    });

    runTest('Multilingual Hindi Speech Generation for Payment Success', () => {
        const prompt = voice.getSpeechPrompt('success', { amount: 250 }, 'hi');
        assert.ok(prompt.text.includes('250 rupaye prapt hue'), `Prompt text: ${prompt.text}`);
        assert.strictEqual(prompt.lang, 'hi-IN');
    });

    runTest('Multilingual Hindi Speech Generation for Fraud Alert', () => {
        const prompt = voice.getSpeechPrompt('fraud_alert', { reason: 'Fake screenshot' }, 'hi');
        assert.ok(prompt.text.includes('Chetawani: Sandigdh payment pakda gaya'), `Prompt text: ${prompt.text}`);
    });

    // -------------------------------------------------------------
    // SUMMARY
    // -------------------------------------------------------------
    console.log('\n====================================================');
    console.log(` TEST SUMMARY: ${passedTests}/${totalTests} TESTS PASSED`);
    console.log('====================================================');

    if (passedTests === totalTests) {
        console.log('>> ALL TESTS COMPLETED SUCCESSFULLY! <<\n');
        process.exit(0);
    } else {
        console.error('>> SOME TESTS FAILED! <<\n');
        process.exit(1);
    }
}

executeAll();
