#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

#include "ltc_decoder.h"
#include "../include/config.h"

// -----------------------------------------------------------------------------
// Audio graph
// -----------------------------------------------------------------------------

AudioInputI2S          audioIn;
AudioRecordQueue       recordQueue;
AudioSynthWaveformSine testTone;
AudioMixer4            mixerL, mixerR;
AudioOutputI2S         audioOut;
AudioControlSGTL5000   sgtl5000;

AudioConnection patchIn_Q(audioIn,  0, recordQueue, 0);
AudioConnection patchIn_L(audioIn,  0, mixerL,      0);
AudioConnection patchIn_R(audioIn,  1, mixerR,      0);
AudioConnection patchTn_L(testTone, 0, mixerL,      1);
AudioConnection patchTn_R(testTone, 0, mixerR,      1);
AudioConnection patchMx_L(mixerL,   0, audioOut,    0);
AudioConnection patchMx_R(mixerR,   0, audioOut,    1);

// -----------------------------------------------------------------------------
// LTC decoder
// -----------------------------------------------------------------------------

LTCDecoder ltcDecoder;

// -----------------------------------------------------------------------------

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    while (!Serial && millis() < 3000) {}

    AudioMemory(AUDIO_MEMORY_BLOCKS);

    // -- Codec -----------------------------------------------------------------
    bool codecOK = sgtl5000.enable();
    delay(200);
    sgtl5000.inputSelect(AUDIO_INPUT_SOURCE);
    sgtl5000.lineInLevel(AUDIO_LINEIN_LEVEL);
    sgtl5000.volume(AUDIO_VOLUME);
    mixerL.gain(0, 1.0f);  mixerR.gain(0, 1.0f);
    mixerL.gain(1, 0.0f);  mixerR.gain(1, 0.0f);

    // -- Banner ----------------------------------------------------------------
    Serial.println("-----------------------------------------");
    Serial.println("  Teensy 4.1  LTC Timecode Reader");
    Serial.println("-----------------------------------------");
    Serial.printf ("  Codec  : %s\n", codecOK ? "OK" : "FAILED");
    Serial.printf ("  FPS    : %u\n", LTC_FRAMERATE);
    Serial.println("-----------------------------------------");

    if (!codecOK) { recordQueue.begin(); return; }

    // -- Quick test tone (L then R, 500ms each) --------------------------------
    testTone.frequency(1000.0f);  testTone.amplitude(0.5f);
    mixerL.gain(1, 0.8f);  mixerR.gain(1, 0.0f);  delay(500);
    mixerL.gain(1, 0.0f);  mixerR.gain(1, 0.8f);  delay(500);
    testTone.amplitude(0.0f);
    mixerL.gain(1, 0.0f);  mixerR.gain(1, 0.0f);

    recordQueue.begin();
}

// -----------------------------------------------------------------------------

void loop()
{
    // -- Audio blocks -> LTC decoder -------------------------------------------
    while (recordQueue.available() > 0) {
        const int16_t* block = recordQueue.readBuffer();
        ltcDecoder.processSamples(block, AUDIO_BLOCK_SAMPLES);
        recordQueue.freeBuffer();
    }

    // -- LTC frame decoded: print timecode to serial ---------------------------
    if (ltcDecoder.isFrameReady()) {
        const LTCTimecode& tc = ltcDecoder.getTimecode();
        if (tc.valid) {
            char buf[20];
            snprintf(buf, sizeof(buf), "%02u:%02u:%02u%c%02u",
                     tc.hours, tc.minutes, tc.seconds,
                     tc.dropFrame ? ';' : ':', tc.frames);
            Serial.println(buf);
        }
        ltcDecoder.reset();
    }

#ifdef DEBUG_AUDIO_LEVEL
    static uint32_t lastPeak = 0;
    if (millis() - lastPeak > 500) {
        lastPeak = millis();
        // Read peak from the record queue's input
        Serial.printf("Audio blocks free: %u\n", AudioMemoryUsageMax());
    }
#endif
}
