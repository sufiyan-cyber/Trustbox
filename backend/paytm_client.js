/**
 * ============================================================================
 * PAYTM PAYMENT GATEWAY & UPI VERIFICATION ADAPTER
 * ============================================================================
 * Supports:
 * 1. Live Paytm Sandbox APIs (https://securegw-stage.paytm.in)
 * 2. Instant Mock UPI Ledger for zero-dependency local testing
 * 3. Paytm Server-to-Server (S2S) Payment Notification Webhook
 */

const crypto = require('crypto');
const https = require('https');

class PaytmClient {
    constructor(options = {}) {
        this.merchantId = options.merchantId || process.env.PAYTM_MID || 'M12345678';
        this.merchantKey = options.merchantKey || process.env.PAYTM_MERCHANT_KEY || 'MOCK_PAYTM_SECRET_KEY_2026';
        this.environment = options.environment || process.env.PAYTM_ENVIRONMENT || 'MOCK'; // 'MOCK' or 'SANDBOX'
        this.sandboxHost = 'securegw-stage.paytm.in';

        // In-memory ledger of verified Paytm transactions
        this.transactions = new Map();
        this.seedInitialTransactions();
    }

    /**
     * Seeds initial test transactions representing genuine settled payments
     */
    seedInitialTransactions() {
        const sampleTxns = [
            {
                orderId: 'ORD-1001',
                mid: this.merchantId,
                txnId: 'PTM-UPI-89210982',
                bankTxnId: 'UPI-AXIS-20260917-001',
                amount: 250.00,
                status: 'TXN_SUCCESS',
                payerUpi: 'rajat.verma@okhdfcbank',
                payerName: 'Rajat Verma',
                timestamp: new Date(Date.now() - 2 * 60 * 1000).toISOString(),
                responseCode: '01',
                responseMsg: 'Txn Successful'
            },
            {
                orderId: 'ORD-1002',
                mid: this.merchantId,
                txnId: 'PTM-UPI-89210983',
                bankTxnId: 'UPI-SBIN-20260917-002',
                amount: 50.00,
                status: 'TXN_SUCCESS',
                payerUpi: 'priya.sharma@paytm',
                payerName: 'Priya Sharma',
                timestamp: new Date(Date.now() - 5 * 60 * 1000).toISOString(),
                responseCode: '01',
                responseMsg: 'Txn Successful'
            },
            {
                orderId: 'ORD-1003',
                mid: this.merchantId,
                txnId: 'PTM-UPI-89210984',
                bankTxnId: 'UPI-ICIC-20260917-003',
                amount: 500.00,
                status: 'TXN_SUCCESS',
                payerUpi: 'amit.kumar@icici',
                payerName: 'Amit Kumar',
                timestamp: new Date(Date.now() - 12 * 60 * 1000).toISOString(),
                responseCode: '01',
                responseMsg: 'Txn Successful'
            }
        ];

        for (const txn of sampleTxns) {
            this.transactions.set(txn.orderId, txn);
        }
    }

    /**
     * Generates HMAC-SHA256 signature as per Paytm Developer Documentation
     */
    generateChecksum(params, key = this.merchantKey) {
        if (typeof params === 'object') {
            const sortedKeys = Object.keys(params).sort();
            const dataString = sortedKeys.map(k => `${k}=${params[k]}`).join('&');
            return crypto.createHmac('sha256', key).update(dataString).digest('hex');
        }
        return crypto.createHmac('sha256', key).update(String(params)).digest('hex');
    }

    /**
     * Verifies HMAC signature for request authenticity
     */
    verifyChecksum(params, checksum, key = this.merchantKey) {
        const expected = this.generateChecksum(params, key);
        return expected === checksum;
    }

    /**
     * Injects a real-time incoming UPI payment into the merchant ledger
     */
    recordPayment({ mid, orderId, amount, payerUpi = 'customer@upi', payerName = 'Verified Customer' }) {
        const id = orderId || `ORD-${Date.now().toString().slice(-4)}`;
        const record = {
            orderId: id,
            mid: mid || this.merchantId,
            txnId: `PTM-UPI-${Date.now()}`,
            bankTxnId: `UPI-NPCI-${Date.now()}`,
            amount: parseFloat(amount),
            status: 'TXN_SUCCESS',
            payerUpi: payerUpi,
            payerName: payerName,
            timestamp: new Date().toISOString(),
            responseCode: '01',
            responseMsg: 'Txn Successful'
        };
        this.transactions.set(id, record);
        return record;
    }

    /**
     * Handles real Paytm S2S Payment Webhook from Paytm servers
     */
    handlePaytmWebhook(webhookData) {
        const orderId = webhookData.ORDERID || webhookData.orderId;
        const txnAmount = parseFloat(webhookData.TXNAMOUNT || webhookData.amount || 0);
        const status = (webhookData.STATUS === 'TXN_SUCCESS') ? 'TXN_SUCCESS' : 'TXN_FAILURE';

        const record = {
            orderId,
            mid: webhookData.MID || this.merchantId,
            txnId: webhookData.TXNID || `PTM-UPI-${Date.now()}`,
            bankTxnId: webhookData.BANKTXNID || `UPI-${Date.now()}`,
            amount: txnAmount,
            status,
            payerUpi: webhookData.GATEWAYNAME || 'paytm_app_user',
            payerName: 'Paytm App Customer',
            timestamp: new Date().toISOString(),
            responseCode: webhookData.RESPCODE || '01',
            responseMsg: webhookData.RESPMSG || 'Txn Successful'
        };

        if (status === 'TXN_SUCCESS') {
            this.transactions.set(orderId, record);
        }
        return record;
    }

    /**
     * Queries Paytm Gateway for transaction status
     */
    async checkTransactionStatus(mid, orderId) {
        // If real Sandbox mode is enabled and credentials are set
        if (this.environment === 'SANDBOX' && this.merchantId && !this.merchantId.startsWith('M1234')) {
            try {
                return await this.queryLivePaytmSandbox(mid || this.merchantId, orderId);
            } catch (err) {
                console.warn('[PAYTM] Sandbox API call failed, falling back to local verification:', err.message);
            }
        }

        // 1. Direct Order ID match in ledger
        if (orderId && this.transactions.has(orderId)) {
            const txn = this.transactions.get(orderId);
            return {
                found: true,
                status: txn.status,
                amount: txn.amount,
                orderId: txn.orderId,
                txnId: txn.txnId,
                timestamp: txn.timestamp,
                payerUpi: txn.payerUpi,
                payerName: txn.payerName
            };
        }

        // 2. Query latest transaction for this MID (for "Kitna aaya?" voice queries)
        const merchantTxns = Array.from(this.transactions.values())
            .filter(t => !mid || t.mid === mid)
            .sort((a, b) => new Date(b.timestamp).getTime() - new Date(a.timestamp).getTime());

        if (merchantTxns.length > 0 && (!orderId || orderId === 'LATEST')) {
            const latest = merchantTxns[0];
            return {
                found: true,
                status: latest.status,
                amount: latest.amount,
                orderId: latest.orderId,
                txnId: latest.txnId,
                timestamp: latest.timestamp,
                payerUpi: latest.payerUpi,
                payerName: latest.payerName,
                isLatestFallback: true
            };
        }

        // 3. No matching transaction found
        return {
            found: false,
            status: 'TXN_FAILURE',
            orderId: orderId,
            reason: 'NO_RECORD_FOUND_IN_PAYTM_GATEWAY'
        };
    }

    /**
     * Live HTTP query to Paytm Sandbox Order Status API (v3)
     */
    queryLivePaytmSandbox(mid, orderId) {
        return new Promise((resolve, reject) => {
            const postData = JSON.stringify({
                body: {
                    mid: mid,
                    orderId: orderId
                },
                head: {
                    signature: this.generateChecksum(orderId, this.merchantKey)
                }
            });

            const options = {
                hostname: this.sandboxHost,
                port: 443,
                path: `/v3/order/status`,
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Content-Length': Buffer.byteLength(postData)
                }
            };

            const req = https.request(options, (res) => {
                let data = '';
                res.on('data', chunk => data += chunk);
                res.on('end', () => {
                    try {
                        const json = JSON.parse(data);
                        const body = json.body || {};
                        if (body.resultInfo && body.resultInfo.resultStatus === 'TXN_SUCCESS') {
                            resolve({
                                found: true,
                                status: 'TXN_SUCCESS',
                                amount: parseFloat(body.txnAmount || 0),
                                orderId: body.orderId,
                                txnId: body.txnId,
                                timestamp: body.txnDate || new Date().toISOString()
                            });
                        } else {
                            resolve({
                                found: false,
                                status: 'TXN_FAILURE',
                                orderId: orderId,
                                reason: body.resultInfo ? body.resultInfo.resultMsg : 'Transaction not found'
                            });
                        }
                    } catch (e) {
                        reject(e);
                    }
                });
            });

            req.on('error', reject);
            req.setTimeout(4000, () => {
                req.destroy();
                reject(new Error('Paytm Sandbox Timeout'));
            });

            req.write(postData);
            req.end();
        });
    }

    /**
     * Lists all transactions for dashboard
     */
    getAllTransactions(mid = null) {
        const all = Array.from(this.transactions.values());
        if (mid) {
            return all.filter(t => t.mid === mid);
        }
        return all.sort((a, b) => new Date(b.timestamp).getTime() - new Date(a.timestamp).getTime());
    }
}

module.exports = PaytmClient;
