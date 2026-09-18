/**
 * ============================================================================
 * PAYTM TRUSTBOX: FRAUD DETECTION ENGINE (ML & HEURISTICS)
 * ============================================================================
 * Implements calibrated fraud risk scoring combining hard rules, heuristic
 * signals, and risk clustering as specified in the TrustBox PRD.
 */

const CogneeService = require('./cognee_service');

class FraudEngine {
    constructor(options = {}) {
        this.cognee = options.cogneeService || new CogneeService();

        // High-risk known fraudulent UPI IDs / cluster actors (simulated Cognee graph)
        this.knownFraudsters = new Set([
            'scammer.upi@fakebank',
            'quickcash.refund@ybl',
            'phish.alert@okhdfcbank',
            'fast.pay.bogus@paytm'
        ]);

        // Recent dispute frequency map by merchant/payer
        this.disputeVelocity = new Map();
    }

    /**
     * Records a dispute against a payer or merchant for velocity calculation
     */
    recordDispute(payerUpi, merchantId) {
        if (payerUpi) {
            const current = this.disputeVelocity.get(payerUpi) || 0;
            this.disputeVelocity.set(payerUpi, current + 1);
        }
    }

    /**
     * Evaluates a payment verification query and computes a fraud risk score (0.0 to 1.0)
     * 
     * @param {Object} queryContext
     *   - merchantId: string
     *   - orderId: string
     *   - expectedAmount: number (claimed by customer/merchant)
     *   - gatewayResult: object (from PaytmClient)
     *   - soundSpoofed: boolean (optional test flag or acoustic anomaly)
     *   - customerUpi: string
     *   - claimedTimestamp: ISO string
     */
    evaluateRisk(queryContext) {
        const {
            merchantId,
            orderId,
            expectedAmount,
            gatewayResult,
            soundSpoofed = false,
            customerUpi,
            claimedTimestamp
        } = queryContext;

        const factors = [];
        let fraudScore = 0.05; // Base low baseline for any digital txn

        // -------------------------------------------------------------
        // RULE 1: Missing Transaction in Paytm Gateway (Fake Screenshot)
        // -------------------------------------------------------------
        if (!gatewayResult || !gatewayResult.found || gatewayResult.status !== 'TXN_SUCCESS') {
            fraudScore = 1.0;
            factors.push({
                code: 'NO_GATEWAY_RECORD',
                weight: 1.0,
                description: 'No matching transaction record found in Paytm UPI Gateway (Fake Screenshot / Ghost Txn)'
            });

            return {
                fraudScore: 1.0,
                riskTier: 'FRAUD',
                isFlagged: true,
                factors: factors,
                explanation: 'CRITICAL: No payment record exists in Paytm Gateway. High probability of fake screenshot scam.',
                recommendation: 'DO NOT hand over goods. Request genuine payment or press 🔺 Dispute to notify ops.'
            };
        }

        // -------------------------------------------------------------
        // RULE 2: Amount Mismatch (Overpayment / Doctored Screen Scam)
        // -------------------------------------------------------------
        const actualAmount = gatewayResult.amount;
        if (expectedAmount && expectedAmount > 0) {
            const amountDiff = Math.abs(expectedAmount - actualAmount);
            if (amountDiff > 1.0) { // Tolerance for minor round-off
                const penalty = 0.85;
                fraudScore = Math.max(fraudScore, penalty);
                factors.push({
                    code: 'AMOUNT_MISMATCH',
                    weight: penalty,
                    description: `Claimed amount (₹${expectedAmount}) does not match gateway credit (₹${actualAmount})`
                });
            }
        }

        // -------------------------------------------------------------
        // RULE 3: Audio Spoofing (Customer played fake chime app)
        // -------------------------------------------------------------
        if (soundSpoofed) {
            fraudScore = Math.max(fraudScore, 0.95);
            factors.push({
                code: 'AUDIO_SPOOF_DETECTED',
                weight: 0.95,
                description: 'Sound alert played on customer phone without corresponding server push event'
            });
        }

        // -------------------------------------------------------------
        // RULE 4: Cognee Fraud Memory Graph & Scam Cluster Anomaly
        // -------------------------------------------------------------
        const payer = gatewayResult.payerUpi || customerUpi;
        if (payer) {
            const graphAnalysis = this.cognee.analyzeRiskGraphSync({ customerUpi: payer, merchantId });
            if (graphAnalysis.isClusterMatch || this.knownFraudsters.has(payer.toLowerCase())) {
                const penalty = graphAnalysis.graphRiskFactor || 0.40;
                fraudScore = Math.min(1.0, fraudScore + penalty);
                factors.push({
                    code: 'COGNEE_SCAM_CLUSTER_DETECTED',
                    weight: penalty,
                    description: graphAnalysis.insight || `Payer UPI (${payer}) flagged in Cognee fraud memory graph`
                });
            }
        }

        // -------------------------------------------------------------
        // RULE 5: Repeat Dispute Velocity
        // -------------------------------------------------------------
        if (payer && this.disputeVelocity.get(payer) > 0) {
            const count = this.disputeVelocity.get(payer);
            const penalty = Math.min(0.40, count * 0.20);
            fraudScore = Math.min(1.0, fraudScore + penalty);
            factors.push({
                code: 'HIGH_DISPUTE_VELOCITY',
                weight: penalty,
                description: `Payer associated with ${count} prior dispute(s) today`
            });
        }

        // -------------------------------------------------------------
        // RULE 6: Temporal Anomaly (Stale claim)
        // -------------------------------------------------------------
        if (gatewayResult.timestamp) {
            const txnTime = new Date(gatewayResult.timestamp).getTime();
            const now = Date.now();
            const ageMinutes = (now - txnTime) / (1000 * 60);

            if (ageMinutes > 30) {
                fraudScore = Math.min(1.0, fraudScore + 0.15);
                factors.push({
                    code: 'STALE_TRANSACTION',
                    weight: 0.15,
                    description: `Transaction occurred ${Math.round(ageMinutes)} mins ago (potential replay)`
                });
            }
        }

        // Calibrate final score to 2 decimal places [0.00, 1.00]
        fraudScore = parseFloat(Math.min(1.0, Math.max(0.0, fraudScore)).toFixed(2));

        // Assign Risk Tier according to PRD thresholds:
        // < 0.3 = Green (Safe), 0.3 - 0.7 = Yellow (Caution), > 0.7 = Red (Fraud)
        let riskTier = 'SAFE';
        let isFlagged = false;
        let explanation = 'Transaction is verified and safe.';
        let recommendation = 'Payment confirmed. Safe to hand over merchandise.';

        if (fraudScore > 0.70) {
            riskTier = 'FRAUD';
            isFlagged = true;
            explanation = factors.map(f => f.description).join('; ');
            recommendation = '⚠️ High risk transaction! Inspect screen and verify before release.';
        } else if (fraudScore >= 0.30) {
            riskTier = 'CAUTION';
            isFlagged = false;
            explanation = factors.map(f => f.description).join('; ');
            recommendation = 'Double-check amount and customer details.';
        }

        return {
            fraudScore,
            riskTier,
            isFlagged,
            factors,
            explanation,
            recommendation
        };
    }
}

module.exports = FraudEngine;
