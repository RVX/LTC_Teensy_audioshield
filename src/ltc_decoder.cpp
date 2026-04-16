#include "ltc_decoder.h"
#include "../include/config.h"
#include <cstring>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────

LTCDecoder::LTCDecoder()
    : _prevSample(0)
    , _samplesPerBit(LTC_SAMPLES_PER_BIT)
    , _halfBit(LTC_SAMPLES_PER_BIT * 0.5f)
    , _sampleCount(0.0f)
    , _polarity(0)
    , _bitCount(0)
    , _inSync(false)
    , _frameReady(false)
    , _totalBits(0)
    , _expectMidBit(false)
{
    memset(_bitBuffer, 0, sizeof(_bitBuffer));
    _timecode.clear();
}

// ─────────────────────────────────────────────────────────────────────────────

void LTCDecoder::processSamples(const int16_t* samples, uint16_t count)
{
    const int16_t threshold = static_cast<int16_t>(LTC_ZC_THRESHOLD * 32767.0f);

    for (uint16_t i = 0; i < count; ++i) {
        int16_t s = samples[i];

        // Detect zero crossing (with hysteresis via threshold)
        bool crossed = false;
        if (_polarity >= 0 && s < -threshold) {
            _polarity = -1;
            crossed   = true;
        } else if (_polarity <= 0 && s > threshold) {
            _polarity = 1;
            crossed   = true;
        }

        if (crossed) {
            _handleCrossing(_sampleCount);
            _sampleCount = 0.0f;
        } else {
            _sampleCount += 1.0f;
        }

        _prevSample = s;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Biphase-mark decoding — 2-state machine:
//
//  State _expectMidBit == false  (at a bit boundary):
//    short interval → mid-bit crossing → emit '1', go to _expectMidBit=true
//    long  interval → no mid-bit → emit '0', stay
//
//  State _expectMidBit == true  (just saw a mid-bit, now expecting boundary):
//    short interval → bit boundary after mid → go to _expectMidBit=false (no emit)
//    long  interval → error, resync → go to _expectMidBit=false

void LTCDecoder::_handleCrossing(float intervalSamples)
{
    // Intervals > 1.5 bit-periods are signal dropouts — hard reset
    if (intervalSamples > _samplesPerBit * 1.5f) {
        _expectMidBit = false;
        return;
    }

    // Threshold between "short" (~half bit) and "long" (~full bit)
    // Use 0.75 * samplesPerBit as the dividing line
    const bool isShort = (intervalSamples < _samplesPerBit * 0.75f);

    if (!_expectMidBit) {
        // At a bit boundary — decide 0 or 1
        if (isShort) {
            // Short: mid-bit crossing → bit '1'
            _pushBit(1);
            _expectMidBit = true;
        } else {
            // Long: next bit boundary crossed, no mid-bit → bit '0'
            _pushBit(0);
        }
    } else {
        // We just emitted a '1'; this crossing is the next bit boundary — consume it
        _expectMidBit = false;
        // (no bit emitted; the boundary crossing itself carries no extra data)
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void LTCDecoder::_pushBit(uint8_t bit)
{
#ifdef DEBUG_LTC_BITS
    Serial.print(bit);
#endif

    if (_bitCount < 80) {
        _bitBuffer[_bitCount++] = bit;
        _totalBits++;
    }

    // Check for sync word when we have at least 80 bits
    if (_bitCount == 80) {
        if (_checkSyncWord()) {
            _decodeFrame();
            _frameReady = true;
        }
        // Shift buffer left by 1 bit, keep searching
        memmove(_bitBuffer, _bitBuffer + 1, 79);
        _bitCount = 79;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// LTC sync word is bits 64-79: 0011 1111 1111 1101 (LSB first in stream)
// ─────────────────────────────────────────────────────────────────────────────

bool LTCDecoder::_checkSyncWord() const
{
    uint16_t word = 0;
    for (uint8_t i = 0; i < 16; ++i) {
        word |= (static_cast<uint16_t>(_bitBuffer[64 + i]) << i);
    }
    return (word == SYNC_WORD);
}

// ─────────────────────────────────────────────────────────────────────────────
// Extract timecode fields from the 80-bit LTC frame (SMPTE 12M bit layout)
// ─────────────────────────────────────────────────────────────────────────────

static uint8_t bitsToInt(const uint8_t* buf, uint8_t start, uint8_t len)
{
    uint8_t val = 0;
    for (uint8_t i = 0; i < len; ++i) {
        val |= (buf[start + i] << i);
    }
    return val;
}

void LTCDecoder::_decodeFrame()
{
    _timecode.frames    = bitsToInt(_bitBuffer, 0, 4) + bitsToInt(_bitBuffer,  8, 2) * 10;
    _timecode.seconds   = bitsToInt(_bitBuffer,16, 4) + bitsToInt(_bitBuffer, 24, 3) * 10;
    _timecode.minutes   = bitsToInt(_bitBuffer,32, 4) + bitsToInt(_bitBuffer, 40, 3) * 10;
    _timecode.hours     = bitsToInt(_bitBuffer,48, 4) + bitsToInt(_bitBuffer, 56, 2) * 10;
    _timecode.dropFrame = (_bitBuffer[10] == 1);
    _timecode.colorFrame= (_bitBuffer[11] == 1);
    _timecode.valid     = true;
}
