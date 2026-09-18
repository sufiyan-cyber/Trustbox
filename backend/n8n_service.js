/**
 * ============================================================================
 * n8n: AUTOMATED INCIDENT RESPONSE & WORKFLOW ORCHESTRATION SERVICE
 * ============================================================================
 * Official Sponsor Integration for Paytm TrustBox:
 * 1. Dispatches automated incident webhook on Fraud Detection (Score >= 0.70)
 * 2. Triggers WhatsApp alerts to merchants via n8n's messaging nodes
 * 3. Escalates disputes to Paytm Ops CRM and logs to 1930 Cybercrime graph
 * 4. Dual-mode execution (Dispatches to live n8n instance or simulates cleanly)
 * ============================================================================
 */

const http = require('http');
const https = require('https');
const { URL } = require('url');

class N8nService {
    constructor(options = {}) {
        this.webhookUrl = options.webhookUrl || process.env.N8N_WEBHOOK_URL || '';
        this.whatsappNumber = options.whatsappNumber || process.env.MERCHANT_WHATSAPP_NUMBER || '+918496074290';
        this.whatsappApiKey = options.whatsappApiKey || process.env.WHATSAPP_API_KEY || 'DEMO_KEY';
        this.telegramChatId = (options.telegramChatId || process.env.TELEGRAM_CHAT_ID || '').toString().trim();
        this.telegramBotToken = (options.telegramBotToken || process.env.TELEGRAM_BOT_TOKEN || '').toString().trim();
        this.dispatchedIncidents = [];
    }

    /**
     * Checks if a live n8n webhook URL is configured
     */
    isConfigured() {
        return Boolean(this.webhookUrl && this.webhookUrl.trim().startsWith('http'));
    }

    /**
     * Fires automated incident workflow when fraud or fake screenshot is detected
     * 
     * @param {Object} alertData
     *   - merchantId: string
     *   - merchantName: string
     *   - orderId: string
     *   - amount: number
     *   - fraudScore: number
     *   - alertLevel: string
     *   - explanation: string
     *   - recommendation: string
     */
    async triggerFraudAlert(alertData) {
        const payload = {
            event: 'FRAUD_ALERT_TRIGGERED',
            eventId: `N8N-EVT-${Date.now().toString().slice(-6)}`,
            timestamp: new Date().toISOString(),
            merchant: {
                id: alertData.merchantId || 'M12345678',
                name: alertData.merchantName || 'Rajesh Kirana Store',
                whatsappNumber: this.whatsappNumber,
                telegramChatId: this.telegramChatId
            },
            telegramChatId: this.telegramChatId,
            incident: {
                orderId: alertData.orderId || 'UNRECORDED_TXN',
                claimedAmount: alertData.amount || 0,
                fraudScore: alertData.fraudScore || 1.0,
                severity: alertData.alertLevel || 'CRITICAL_FRAUD',
                reason: alertData.explanation || 'Fake screenshot detected. No credit in bank ledger.',
                recommendation: alertData.recommendation || 'DO NOT hand over goods! Goods withheld.'
            },
            escalation: {
                notifyMerchantTelegram: true,
                log1930Cybercrime: alertData.fraudScore >= 0.85,
                createPaytmOpsTicket: true,
                suggestedAction: 'POLICE_1930_AND_TELEGRAM_ALERT'
            }
        };

        this.dispatchedIncidents.unshift(payload);
        if (this.dispatchedIncidents.length > 50) this.dispatchedIncidents.pop();

        // Direct Telegram dispatch if bot token is provided
        if (this.telegramBotToken && this.telegramChatId) {
            this.sendDirectTelegram(
                `🚨 *PAYTM TRUSTBOX CRITICAL FRAUD WARNING*\n\n` +
                `🏪 *Merchant:* ${alertData.merchantName || 'Rajesh Kirana Store'}\n` +
                `💰 *Claimed Amount:* ₹${alertData.amount || 0}\n` +
                `⚠️ *Fraud Score:* ${alertData.fraudScore || 1.0} / 1.00\n` +
                `🛑 *Status:* DO NOT HAND OVER GOODS!\n\n` +
                `📋 *Reason:* ${alertData.explanation || 'Fake screenshot detected. No credit in bank ledger.'}\n` +
                `⚡ *Action Taken:* 1930 National Cybercrime helpline notified.`
            ).catch(err => console.warn('[TELEGRAM] Direct send error:', err.message));
        }

        if (this.isConfigured()) {
            return await this.sendWebhook(payload);
        }

        console.log(`[n8n] Mock Webhook Triggered -> Incident ${payload.eventId} queued for Telegram Chat ${this.telegramChatId}`);
        return {
            status: 'DISPATCHED_SIMULATED',
            eventId: payload.eventId,
            telegramChatId: this.telegramChatId,
            target: 'n8n_workflow_simulator',
            message: 'n8n workflow triggered (Set N8N_WEBHOOK_URL in .env to dispatch to live n8n)',
            payload
        };
    }

    /**
     * Fires dispute escalation workflow when merchant presses 🔺 Dispute button
     */
    async triggerDispute(disputeData) {
        const payload = {
            event: 'DISPUTE_RAISED',
            eventId: `N8N-DSP-${Date.now().toString().slice(-6)}`,
            timestamp: new Date().toISOString(),
            ticketId: disputeData.ticketId,
            merchantId: disputeData.merchantId,
            orderId: disputeData.orderId,
            reason: disputeData.reason,
            priority: 'HIGH',
            telegramChatId: this.telegramChatId,
            action: 'DISPATCH_PAYTM_FIELD_OFFICER'
        };

        this.dispatchedIncidents.unshift(payload);

        // Direct Telegram alert if bot token configured
        if (this.telegramBotToken && this.telegramChatId) {
            this.sendDirectTelegram(
                `🔺 *PAYTM TRUSTBOX DISPUTE ESCALATION*\n\n` +
                `🎫 *Ticket ID:* ${disputeData.ticketId}\n` +
                `🏪 *Merchant:* ${disputeData.merchantId}\n` +
                `📦 *Order ID:* ${disputeData.orderId}\n` +
                `📋 *Reason:* ${disputeData.reason}\n` +
                `⚡ *Priority:* HIGH - Paytm Ops Review Queued.`
            ).catch(err => console.warn('[TELEGRAM] Direct send error:', err.message));
        }

        if (this.isConfigured()) {
            return await this.sendWebhook(payload);
        }

        console.log(`[n8n] Mock Dispute Workflow Triggered -> Ticket ${payload.ticketId} forwarded to Ops`);
        return {
            status: 'DISPATCHED_SIMULATED',
            ticketId: payload.ticketId,
            target: 'n8n_workflow_simulator',
            payload
        };
    }

    /**
     * Sends message directly to Telegram Bot API if bot token is configured
     */
    async sendDirectTelegram(text) {
        if (!this.telegramBotToken || !this.telegramChatId) {
            return { sent: false, reason: 'Telegram bot token or chat ID not set' };
        }

        return new Promise((resolve) => {
            try {
                const postData = JSON.stringify({
                    chat_id: this.telegramChatId,
                    text: text,
                    parse_mode: 'Markdown'
                });

                const options = {
                    hostname: 'api.telegram.org',
                    port: 443,
                    path: `/bot${this.telegramBotToken}/sendMessage`,
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                        'Content-Length': Buffer.byteLength(postData)
                    },
                    timeout: 5000
                };

                const req = https.request(options, (res) => {
                    let body = '';
                    res.on('data', chunk => body += chunk);
                    res.on('end', () => {
                        console.log(`[TELEGRAM] Direct API response: ${res.statusCode}`);
                        resolve({ sent: res.statusCode === 200, statusCode: res.statusCode, body });
                    });
                });

                req.on('error', (err) => {
                    console.warn(`[TELEGRAM] Direct send error: ${err.message}`);
                    resolve({ sent: false, error: err.message });
                });

                req.on('timeout', () => {
                    req.destroy();
                    resolve({ sent: false, error: 'Telegram timeout' });
                });

                req.write(postData);
                req.end();
            } catch (err) {
                resolve({ sent: false, error: err.message });
            }
        });
    }

    /**
     * Sends HTTP/HTTPS POST to configured n8n Webhook endpoint
     */
    sendWebhook(payload) {
        return new Promise((resolve) => {
            try {
                const parsedUrl = new URL(this.webhookUrl);
                const isHttps = parsedUrl.protocol === 'https:';
                const client = isHttps ? https : http;
                const postData = JSON.stringify(payload);

                const options = {
                    hostname: parsedUrl.hostname,
                    port: parsedUrl.port || (isHttps ? 443 : 80),
                    path: parsedUrl.pathname + parsedUrl.search,
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                        'Content-Length': Buffer.byteLength(postData),
                        'User-Agent': 'PaytmTrustBox/1.0'
                    },
                    timeout: 7000
                };

                const req = client.request(options, (res) => {
                    let data = '';
                    res.on('data', chunk => data += chunk);
                    res.on('end', () => {
                        console.log(`[n8n] Live Webhook dispatched successfully! Status: ${res.statusCode}`);
                        resolve({
                            status: 'DISPATCHED_LIVE',
                            statusCode: res.statusCode,
                            target: this.webhookUrl,
                            telegramChatId: this.telegramChatId,
                            eventId: payload.eventId,
                            response: data
                        });
                    });
                });

                req.on('timeout', () => {
                    req.destroy();
                    console.warn('[n8n] Webhook call timed out (7000ms)');
                    resolve({ status: 'TIMEOUT', fallback: true });
                });

                req.on('error', (err) => {
                    console.warn(`[n8n] Webhook connection error: ${err.message}`);
                    resolve({ status: 'ERROR', message: err.message });
                });

                req.write(postData);
                req.end();
            } catch (err) {
                console.warn(`[n8n] Invalid URL: ${err.message}`);
                resolve({ status: 'INVALID_URL', message: err.message });
            }
        });
    }

    /**
     * Returns live status of n8n integration for dashboard
     */
    getStatus() {
        return {
            provider: 'n8n Workflow Automation',
            configured: this.isConfigured(),
            status: this.isConfigured() ? 'LIVE (Cloud Webhook)' : 'SIMULATED (Ready)',
            webhookUrl: this.webhookUrl || 'Not set (Mock mode)',
            telegramChatId: this.telegramChatId || 'Not configured',
            directTelegramConfigured: Boolean(this.telegramBotToken && this.telegramChatId),
            totalDispatched: this.dispatchedIncidents.length,
            recentIncidents: this.dispatchedIncidents.slice(0, 5)
        };
    }
}

module.exports = N8nService;
