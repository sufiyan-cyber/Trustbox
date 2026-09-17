#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include "config.h"

enum SoundAlertType {
    SOUND_PAYMENT_SUCCESS,
    SOUND_FRAUD_ALERT,
    SOUND_DISPUTE_RAISED,
    SOUND_BUTTON_CLICK,
    SOUND_WAKE_UP
};

class AudioManager {
public:
    AudioManager();
    void begin();

    // Sound Generation & Tones
    void playSound(SoundAlertType soundType);
    void playTone(unsigned int frequency, unsigned long durationMs);
    void stopTone();

    // Voice & TTS Playback
    void speakText(const char* text, const char* lang = "hi");

    // Microphone Voice Capture
    bool isVoiceTriggerDetected();
    int readMicrophoneLevel();

private:
    bool isMuted;
    void playSuccessChime();
    void playFraudSiren();
    void playDisputeChime();
};

extern AudioManager audio;

#endif // AUDIO_MANAGER_H
