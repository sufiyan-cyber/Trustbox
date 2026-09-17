#include "audio_manager.h"

AudioManager audio;

AudioManager::AudioManager() : isMuted(false) {}

void AudioManager::begin() {
    pinMode(PIN_AUDIO_DAC_OR_BUZZER, OUTPUT);
    digitalWrite(PIN_AUDIO_DAC_OR_BUZZER, LOW);

    pinMode(PIN_MIC_INPUT, INPUT);

    Serial.println(F("[AUDIO] Sound subsystem initialized (Speaker/Buzzer on Pin 25, Mic on Pin 34)."));
}

void AudioManager::playTone(unsigned int frequency, unsigned long durationMs) {
#if defined(ESP32)
    // ESP32 LEDC PWM channel tone generation
    ledcAttach(PIN_AUDIO_DAC_OR_BUZZER, frequency, 8);
    ledcWrite(PIN_AUDIO_DAC_OR_BUZZER, 128); // 50% duty cycle
    delay(durationMs);
    ledcWrite(PIN_AUDIO_DAC_OR_BUZZER, 0);
    ledcDetach(PIN_AUDIO_DAC_OR_BUZZER);
#else
    tone(PIN_AUDIO_DAC_OR_BUZZER, frequency, durationMs);
    delay(durationMs);
    noTone(PIN_AUDIO_DAC_OR_BUZZER);
#endif
}

void AudioManager::stopTone() {
#if defined(ESP32)
    ledcWrite(PIN_AUDIO_DAC_OR_BUZZER, 0);
#else
    noTone(PIN_AUDIO_DAC_OR_BUZZER);
#endif
}

void AudioManager::playSuccessChime() {
    Serial.println(F("[AUDIO] >> Playing Paytm Success Chime (E5 -> G#5 -> B5 -> E6)..."));
    playTone(659, 100); // E5
    delay(30);
    playTone(830, 100); // G#5
    delay(30);
    playTone(987, 120); // B5
    delay(30);
    playTone(1318, 300); // E6
}

void AudioManager::playFraudSiren() {
    Serial.println(F("[AUDIO] >> Playing WARNING FRAUD SIREN (880Hz <-> 440Hz)..."));
    for (int i = 0; i < 3; i++) {
        playTone(880, 120);
        delay(40);
        playTone(440, 120);
        delay(40);
    }
}

void AudioManager::playDisputeChime() {
    Serial.println(F("[AUDIO] >> Playing Dispute Confirmation Chime..."));
    playTone(440, 150);
    delay(50);
    playTone(554, 150);
    delay(50);
    playTone(659, 250);
}

void AudioManager::playSound(SoundAlertType soundType) {
    if (isMuted) return;

    switch (soundType) {
        case SOUND_PAYMENT_SUCCESS:
            playSuccessChime();
            break;
        case SOUND_FRAUD_ALERT:
            playFraudSiren();
            break;
        case SOUND_DISPUTE_RAISED:
            playDisputeChime();
            break;
        case SOUND_BUTTON_CLICK:
            playTone(1200, 30);
            break;
        case SOUND_WAKE_UP:
            playTone(523, 60);
            delay(20);
            playTone(659, 80);
            break;
    }
}

void AudioManager::speakText(const char* text, const char* lang) {
    // Outputs the spoken TTS prompt to Serial console and logs for the merchant
    Serial.printf("[TTS-VOICE (%s)] \" %s \"\n", lang ? lang : "hi", text ? text : "");
}

int AudioManager::readMicrophoneLevel() {
    return analogRead(PIN_MIC_INPUT);
}

bool AudioManager::isVoiceTriggerDetected() {
    int level = readMicrophoneLevel();
    // Simple acoustic threshold trigger
    if (level > 2800) {
        return true;
    }
    return false;
}
