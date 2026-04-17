#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

#include "ltc_decoder.h"
#include "../include/config.h"
#ifdef ENABLE_DISPLAY
#include "../include/display.h"
#endif

// -----------------------------------------------------------------------------
// Audio graph
// -----------------------------------------------------------------------------

AudioInputI2S          audioIn;
AudioRecordQueue       recordQueue;
AudioSynthWaveformSine testTone;
AudioMixer4            mixerL, mixerR;
AudioOutputI2S         audioOut;
AudioControlSGTL5000   sgtl5000;
AudioAnalyzePeak       peakL;

AudioConnection patchIn_Q(audioIn,  0, recordQueue, 0);
AudioConnection patchPk_L(audioIn,  0, peakL,       0);
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
static bool s_codecFailed = false;
#ifdef ENABLE_DISPLAY
LTCDisplay ltcDisplay;
#endif

// -----------------------------------------------------------------------------

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    while (!Serial && millis() < 3000) {}

    AudioMemory(AUDIO_MEMORY_BLOCKS);

    // -- Codec -----------------------------------------------------------------
    bool codecOK = sgtl5000.enable();
    s_codecFailed = !codecOK;
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

#ifdef ENABLE_DISPLAY
    if (!ltcDisplay.begin()) {
        Serial.println("[!!] DISPLAY not found (0x3C)");
    } else {
        ltcDisplay.pushMessage("LTC Timecode Reader");
        ltcDisplay.pushMessage(codecOK ? "Codec: OK" : "Codec: FAILED");
        ltcDisplay.update();
    }
#endif

    if (!codecOK) { return; }

    // -- Quick test tone (L then R, 500ms each) --------------------------------
    testTone.frequency(1000.0f);  testTone.amplitude(0.5f);
    mixerL.gain(1, 0.8f);  mixerR.gain(1, 0.0f);  delay(500);
    mixerL.gain(1, 0.0f);  mixerR.gain(1, 0.8f);  delay(500);
    testTone.amplitude(0.0f);
    mixerL.gain(1, 0.0f);  mixerR.gain(1, 0.0f);

    recordQueue.begin();
}

// -----------------------------------------------------------------------------
// Helper: timecode → absolute frame number (for skip / jump detection)
// Drop-frame correction per SMPTE 12M: frames 00 and 01 are skipped at the
// start of every minute except multiples of 10.
// -----------------------------------------------------------------------------

static uint32_t tcToAbsFrames(const LTCTimecode& tc)
{
    uint32_t abs = ((uint32_t)tc.hours * 3600u + tc.minutes * 60u + tc.seconds)
                   * LTC_FRAMERATE + tc.frames;
    if (tc.dropFrame) {
        uint32_t totalMinutes = (uint32_t)tc.hours * 60u + tc.minutes;
        abs -= 2u * (totalMinutes - totalMinutes / 10u);
    }
    return abs;
}

// -----------------------------------------------------------------------------

void loop()
{
    static bool     ltcPresent    = false;
    static bool     ltcEverSeen   = false;
    static uint32_t lastFrameMs   = 0;
    static uint32_t prevTCFrames  = 0;
    static bool     prevDropFrame = false;
#ifdef DEBUG_HEARTBEAT
    static uint32_t frameSkips = 0;
    static uint32_t tcJumps    = 0;
#endif

    uint32_t now = millis();

    // -- Codec failure: repeated warning, skip all audio processing -----------
    if (s_codecFailed) {
        static uint32_t lastCodecWarn = 0;
        if (now - lastCodecWarn >= 3000) {
            lastCodecWarn = now;
            Serial.println("[!!] CODEC FAILED - check shield / I2C");
#ifdef ENABLE_DISPLAY
            ltcDisplay.pushMessage("!! CODEC FAILED");
#endif
        }
#ifdef ENABLE_DISPLAY
        ltcDisplay.update();
#endif
        return;
    }

    // -- Audio blocks -> LTC decoder -------------------------------------------
    while (recordQueue.available() > 0) {
        const int16_t* block = recordQueue.readBuffer();
        ltcDecoder.processSamples(block, AUDIO_BLOCK_SAMPLES);
        recordQueue.freeBuffer();
    }

    // -- LTC frame decoded -----------------------------------------------------
    if (ltcDecoder.isFrameReady()) {
        const LTCTimecode& tc = ltcDecoder.getTimecode();
        if (tc.valid) {

            // ── Tier 1: acquired / recovered ────────────────────────────────────
            if (!ltcPresent) {
                Serial.printf(ltcEverSeen
                    ? "[OK] LTC recovered @ %02u:%02u:%02u%c%02u\n"
                    : "[OK] LTC acquired  @ %02u:%02u:%02u%c%02u\n",
                    tc.hours, tc.minutes, tc.seconds,
                    tc.dropFrame ? ';' : ':', tc.frames);
#ifdef ENABLE_DISPLAY
                {
                    char dmsg[22];
                    snprintf(dmsg, sizeof(dmsg), ltcEverSeen ? "LTC recovered" : "LTC acquired");
                    ltcDisplay.pushMessage(dmsg);
                }
#endif
                ltcPresent    = true;
                ltcEverSeen   = true;
                prevDropFrame = tc.dropFrame;
                prevTCFrames  = tcToAbsFrames(tc);

            } else {
                // ── Tier 1: drop-frame flag change ────────────────────────────
                if (tc.dropFrame != prevDropFrame) {
                    Serial.printf("[!!] Drop-frame changed: %s -> %s\n",
                        prevDropFrame ? "ON" : "OFF",
                        tc.dropFrame  ? "ON" : "OFF");
#ifdef ENABLE_DISPLAY
                    {
                        char dmsg[22];
                        snprintf(dmsg, sizeof(dmsg), "DF: %s->%s",
                            prevDropFrame ? "ON" : "OFF",
                            tc.dropFrame  ? "ON" : "OFF");
                        ltcDisplay.pushMessage(dmsg);
                    }
#endif
                    prevDropFrame = tc.dropFrame;
                }

                // ── Tier 1: frame skip / TC jump ──────────────────────────────
                uint32_t curAbs = tcToAbsFrames(tc);
                int32_t  diff   = (int32_t)curAbs - (int32_t)prevTCFrames;
                if (diff < 0 || diff > 10) {
                    Serial.printf("[!!] TC jump   -> %02u:%02u:%02u%c%02u\n",
                        tc.hours, tc.minutes, tc.seconds,
                        tc.dropFrame ? ';' : ':', tc.frames);
#ifdef ENABLE_DISPLAY
                    {
                        char dmsg[22];
                        snprintf(dmsg, sizeof(dmsg), "JUMP->%02u:%02u:%02u:%02u",
                            tc.hours, tc.minutes, tc.seconds, tc.frames);
                        ltcDisplay.pushMessage(dmsg);
                    }
#endif
#ifdef DEBUG_HEARTBEAT
                    tcJumps++;
#endif
                } else if (diff > 1) {
                    Serial.printf("[!!] Frame skip: +%d @ %02u:%02u:%02u%c%02u\n",
                        (int)(diff - 1),
                        tc.hours, tc.minutes, tc.seconds,
                        tc.dropFrame ? ';' : ':', tc.frames);
#ifdef ENABLE_DISPLAY
                    {
                        char dmsg[22];
                        snprintf(dmsg, sizeof(dmsg), "SKIP+%d %02u:%02u:%02u:%02u",
                            (int)(diff-1),
                            (unsigned)(tc.hours % 24),
                            (unsigned)(tc.minutes % 60),
                            (unsigned)(tc.seconds % 60),
                            (unsigned)(tc.frames % 100));
                        ltcDisplay.pushMessage(dmsg);
                    }
#endif
#ifdef DEBUG_HEARTBEAT
                    frameSkips++;
#endif
                }
                prevTCFrames = curAbs;
            }
            lastFrameMs = now;

            // ── Normal timecode print ─────────────────────────────────────────────
            char buf[20];
            snprintf(buf, sizeof(buf), "%02u:%02u:%02u%c%02u",
                     tc.hours, tc.minutes, tc.seconds,
                     tc.dropFrame ? ';' : ':', tc.frames);
            Serial.println(buf);
#ifdef ENABLE_DISPLAY
            ltcDisplay.setTimecode(tc.hours, tc.minutes, tc.seconds,
                                   tc.frames, tc.dropFrame, true);
#endif
        }
        ltcDecoder.reset();
    }

    // ── Tier 1: LTC lost ────────────────────────────────────────────────────────────
    if (ltcPresent && (now - lastFrameMs) > LTC_LOSS_TIMEOUT_MS) {
        Serial.printf("[!!] LTC LOST (%.1fs silence)\n",
            (now - lastFrameMs) / 1000.0f);
        ltcPresent = false;
#ifdef ENABLE_DISPLAY
        ltcDisplay.setTimecode(0,0,0,0,false,false);
        ltcDisplay.pushMessage("!! LTC LOST");
#endif
    }

#ifdef DEBUG_HEARTBEAT
    // ── Tier 2: periodic stat line ───────────────────────────────────────────────
    static uint32_t lastStat = 0;
    if (now - lastStat >= HEARTBEAT_INTERVAL_MS) {
        lastStat = now;
        float level = 0.0f;
        if (peakL.available()) level = peakL.read();
        Serial.printf("[STAT] LTC:%-3s  lvl:%3.0f%%  skips:%-3u  jumps:%-3u  zcErr:%-4u  mem:%2u(pk:%2u)/%u  cpu:%.1f%%\n",
            ltcPresent ? "OK" : "---",
            level * 100.0f,
            frameSkips,
            tcJumps,
            ltcDecoder.getZcResets(),
            AudioMemoryUsage(),
            AudioMemoryUsageMax(),
            AUDIO_MEMORY_BLOCKS,
            AudioProcessorUsage());
        frameSkips = 0;
        tcJumps    = 0;
        ltcDecoder.resetZcResets();
    }
#endif

#ifdef ENABLE_DISPLAY
    static float s_displayLevel = 0.0f;
    if (peakL.available()) s_displayLevel = peakL.read();
    ltcDisplay.setStatus(ltcPresent, s_displayLevel,
                         AudioMemoryUsage(), AudioMemoryUsageMax(),
                         AudioProcessorUsage(),
                         ltcDecoder.getZcResets());
    ltcDisplay.update();
#endif
}
