#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

struct VerifyResult {
    bool success;
    String status;      // "confirmed", "not_found", "flagged_fraud"
    double amount;
    float fraudScore;
    String message;
    String ttsText;
    String alertLevel;  // "SAFE", "CAUTION", "FRAUD"
    String ticketId;    // for disputes
    String rawError;
};

class NetworkClient {
public:
    NetworkClient();
    void begin(const char* ssid = DEFAULT_WIFI_SSID, const char* pass = DEFAULT_WIFI_PASS);
    bool isConnected();
    void setServerUrl(const String& url);
    String getServerUrl() const { return serverUrl; }
    String getIpAddress() const;

    // API Verification Methods
    VerifyResult verifyPayment(const char* merchantId, const char* orderId = nullptr, double expectedAmount = 0.0);
    VerifyResult sendVoiceQuery(const char* merchantId, const char* voiceQueryText);
    VerifyResult reportDispute(const char* merchantId, const char* orderId, const char* reason = "Merchant flagged fraud attempt");

private:
    String serverUrl;
    VerifyResult executePostWithRetry(const String& endpoint, const String& jsonPayload);
};

extern NetworkClient network;

#endif // NETWORK_CLIENT_H
