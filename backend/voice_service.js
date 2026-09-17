/**
 * ============================================================================
 * PAYTM TRUSTBOX: MULTILINGUAL VOICE & SPEECH SERVICE
 * ============================================================================
 * Handles Hindi and English intent parsing for voice queries ("Kitna aaya?")
 * and speech prompt generation for low-literacy Tier-2/3 merchants.
 */

class VoiceService {
    constructor() {
        // Intent patterns for Hindi and English voice input
        this.intents = [
            {
                name: 'QUERY_LATEST_PAYMENT',
                regex: /(kitna|aaya|kya aaya|check|status|verify|payment aaya|paise aaye|how much|balance)/i
            },
            {
                name: 'RAISE_DISPUTE',
                regex: /(dispute|complaint|shikayat|problem|galat|dhokha|fraud|help)/i
            },
            {
                name: 'QUERY_SPECIFIC_AMOUNT',
                regex: /(kitna aaya|check)?\s*(?:rs|inr|rupaye|rupees)?\s*(\d+(?:\.\d+)?)/i
            }
        ];
    }

    /**
     * Parses a spoken or transcribed voice query to extract intent and target amount
     */
    parseVoiceQuery(text) {
        const queryText = (text || '').trim();

        // Check for specific amount mention (e.g. "500 aaya kya?", "Check 250")
        const amountMatch = queryText.match(/(\d+(?:\.\d+)?)/);
        const mentionedAmount = amountMatch ? parseFloat(amountMatch[1]) : null;

        for (const intent of this.intents) {
            if (intent.regex.test(queryText)) {
                return {
                    matched: true,
                    intent: intent.name,
                    rawQuery: queryText,
                    amount: mentionedAmount
                };
            }
        }

        // Default intent: query latest payment
        return {
            matched: true,
            intent: 'QUERY_LATEST_PAYMENT',
            rawQuery: queryText,
            amount: mentionedAmount
        };
    }

    /**
     * Generates localized speech prompts for TrustBox speaker output
     * 
     * @param {string} type 'success' | 'not_found' | 'fraud_alert' | 'dispute' | 'network_error'
     * @param {object} data { amount, orderId, reason, ticketId }
     * @param {string} lang 'hi' (Hindi) | 'en' (English)
     */
    getSpeechPrompt(type, data = {}, lang = 'hi') {
        const { amount = 0, ticketId = '', reason = '' } = data;

        if (lang === 'hi') {
            switch (type) {
                case 'success':
                    return {
                        text: `Paytm TrustBox: ${Math.round(amount)} rupaye prapt hue.`,
                        lang: 'hi-IN',
                        alertTone: 'PAYMENT_CHIME'
                    };
                case 'not_found':
                    return {
                        text: 'Koi payment nahi mila. Kripya customer ka screen mat dekhiye.',
                        lang: 'hi-IN',
                        alertTone: 'BEEP'
                    };
                case 'fraud_alert':
                    return {
                        text: `Chetawani: Sandigdh payment pakda gaya. ${reason || ''}. Kripya laal dispute button dabayein.`,
                        lang: 'hi-IN',
                        alertTone: 'SIREN'
                    };
                case 'dispute':
                    return {
                        text: `Dispute darj ho gaya hai. Ticket number ${ticketId}. Sahayata team sampark karegi.`,
                        lang: 'hi-IN',
                        alertTone: 'CONFIRM'
                    };
                case 'network_error':
                    return {
                        text: 'Server se sampark nahi ho saka. Kripya Wi-Fi connection check karein.',
                        lang: 'hi-IN',
                        alertTone: 'ERROR'
                    };
                default:
                    return { text: 'Paytm TrustBox surakshit hai.', lang: 'hi-IN', alertTone: 'NONE' };
            }
        } else {
            // English Prompts
            switch (type) {
                case 'success':
                    return {
                        text: `Paytm TrustBox: Payment of ₹${amount} confirmed.`,
                        lang: 'en-IN',
                        alertTone: 'PAYMENT_CHIME'
                    };
                case 'not_found':
                    return {
                        text: 'No payment record found. Do not release goods.',
                        lang: 'en-IN',
                        alertTone: 'BEEP'
                    };
                case 'fraud_alert':
                    return {
                        text: `Warning: Possible fraudulent transaction. ${reason || ''}. Press dispute button to escalate.`,
                        lang: 'en-IN',
                        alertTone: 'SIREN'
                    };
                case 'dispute':
                    return {
                        text: `Dispute registered. Ticket ID ${ticketId}. Operations team will contact you.`,
                        lang: 'en-IN',
                        alertTone: 'CONFIRM'
                    };
                case 'network_error':
                    return {
                        text: 'Unable to connect to server. Please check your network.',
                        lang: 'en-IN',
                        alertTone: 'ERROR'
                    };
                default:
                    return { text: 'Paytm TrustBox is ready.', lang: 'en-IN', alertTone: 'NONE' };
            }
        }
    }
}

module.exports = VoiceService;
