#include "network_client.h"

NetworkClient network;

NetworkClient::NetworkClient() : serverUrl(DEFAULT_SERVER_URL) {}

void NetworkClient::begin(const char* ssid, const char* pass) {
    Serial.printf("[NETWORK] Connecting to Wi-Fi SSID: %s...\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 15) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.printf("[NETWORK] Connected! IP Address: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println();
        Serial.println(F("[NETWORK] Wi-Fi connection pending. Operating in offline/diagnostic mode."));
    }
}

bool NetworkClient::isConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

void NetworkClient::setServerUrl(const String& url) {
    serverUrl = url;
}

String NetworkClient::getIpAddress() const {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0 (Offline)";
}

VerifyResult NetworkClient::executePostWithRetry(const String& endpoint, const String& jsonPayload) {
    VerifyResult result;
    result.success = false;
    result.amount = 0.0;
    result.fraudScore = 0.0f;

    String fullUrl = serverUrl + endpoint;
    Serial.printf("[NETWORK] POST to %s\n", fullUrl.c_str());
    Serial.printf("[NETWORK] Payload: %s\n", jsonPayload.c_str());

    for (int attempt = 0; attempt < MAX_HTTP_RETRIES; attempt++) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.printf("[NETWORK] WiFi not connected. Attempt %d/%d.\n", attempt + 1, MAX_HTTP_RETRIES);
        } else {
            HTTPClient http;
            http.begin(fullUrl);
            http.addHeader("Content-Type", "application/json");
            http.setTimeout(HTTP_TIMEOUT_MS);

            int httpCode = http.POST(jsonPayload);
            if (httpCode > 0) {
                String response = http.getString();
                Serial.printf("[NETWORK] Response (%d): %s\n", httpCode, response.c_str());

                // Parse JSON response
                StaticJsonDocument<1024> doc;
                DeserializationError err = deserializeJson(doc, response);
                if (!err) {
                    result.success = (httpCode == 200 || httpCode == 201);
                    result.status = doc["status"].as<String>();
                    result.amount = doc["amount"] | 0.0;
                    result.fraudScore = doc["fraudScore"] | 0.0f;
                    result.message = doc["message"].as<String>();
                    result.ttsText = doc["ttsText"] | doc["message"].as<String>();
                    result.alertLevel = doc["alertLevel"].as<String>();
                    result.ticketId = doc["ticketId"].as<String>();
                    http.end();
                    return result;
                } else {
                    Serial.printf("[NETWORK] JSON Parse Error: %s\n", err.c_str());
                }
            } else {
                Serial.printf("[NETWORK] HTTP error on attempt %d: %s\n", attempt + 1, http.errorToString(httpCode).c_str());
            }
            http.end();
        }

        // Exponential backoff delay
        if (attempt < MAX_HTTP_RETRIES - 1) {
            Serial.printf("[NETWORK] Retrying in %lu ms...\n", RETRY_DELAYS_MS[attempt]);
            delay(RETRY_DELAYS_MS[attempt]);
        }
    }

    result.rawError = "Connection failed after retries";
    return result;
}

VerifyResult NetworkClient::verifyPayment(const char* merchantId, const char* orderId, double expectedAmount) {
    StaticJsonDocument<256> doc;
    doc["eventType"] = "verify_payment";
    doc["merchantId"] = merchantId;
    doc["timestamp"] = millis(); // ESP32 uptime or ISO if NTP synced
    if (orderId && strlen(orderId) > 0) {
        doc["orderId"] = orderId;
    }
    if (expectedAmount > 0.0) {
        JsonObject payload = doc.createNestedObject("payload");
        payload["amount"] = expectedAmount;
    }

    String jsonString;
    serializeJson(doc, jsonString);
    return executePostWithRetry(API_VERIFY_ENDPOINT, jsonString);
}

VerifyResult NetworkClient::sendVoiceQuery(const char* merchantId, const char* voiceQueryText) {
    StaticJsonDocument<256> doc;
    doc["eventType"] = "voice_query";
    doc["merchantId"] = merchantId;
    doc["query"] = voiceQueryText;
    doc["timestamp"] = millis();

    String jsonString;
    serializeJson(doc, jsonString);
    return executePostWithRetry(API_VOICE_ENDPOINT, jsonString);
}

VerifyResult NetworkClient::reportDispute(const char* merchantId, const char* orderId, const char* reason) {
    StaticJsonDocument<256> doc;
    doc["eventType"] = "report_dispute";
    doc["merchantId"] = merchantId;
    doc["orderId"] = orderId ? orderId : "LAST_UNCONFIRMED_TXN";
    doc["reason"] = reason;
    doc["timestamp"] = millis();

    String jsonString;
    serializeJson(doc, jsonString);
    return executePostWithRetry(API_DISPUTE_ENDPOINT, jsonString);
}
