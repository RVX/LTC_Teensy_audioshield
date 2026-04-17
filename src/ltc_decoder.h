#pragma once

#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// LTC Timecode struct
// ─────────────────────────────────────────────────────────────────────────────

struct LTCTimecode {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t frames;
    bool    dropFrame;
    bool    colorFrame;
    bool    valid;       // true when a complete, verified frame was decoded

    void clear() {
        hours = minutes = seconds = frames = 0;
        dropFrame = colorFrame = valid = false;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// LTCDecoder — biphase-mark decoder operating on int16_t audio samples
//
// Usage:
//   1. Construct once.
//   2. Call processSamples() with each audio block received from the
//      AudioRecordQueue (int16_t[], AUDIO_BLOCK_SAMPLES long).
//   3. Check isFrameReady(); if true, call getTimecode() and reset().
// ─────────────────────────────────────────────────────────────────────────────

class LTCDecoder {
public:
    LTCDecoder();

    // Feed a block of raw 16-bit audio samples into the decoder.
    // Call this every time the AudioRecordQueue has a block available.
    void processSamples(const int16_t* samples, uint16_t count);

    // Returns true when a full, valid 80-bit LTC frame has been decoded.
    bool isFrameReady() const { return _frameReady; }

    // Retrieve the last decoded timecode.  Call after isFrameReady() == true.
    const LTCTimecode& getTimecode() const { return _timecode; }

    // Clear the ready flag so the decoder can accept the next frame.
    void reset() { _frameReady = false; }

    // Returns total bits decoded since startup (debug)
    uint32_t getBitsDecoded() const { return _totalBits; }

    // Returns ZC timeout count (signal dropouts) since last resetZcResets().
    uint32_t getZcResets() const { return _zcResets; }
    void     resetZcResets()     { _zcResets = 0; }

private:
    // ── Biphase / zero-crossing state ────────────────────────────────────────
    float    _samplesPerBit;       // nominal samples per LTC bit
    float    _sampleCount;         // fractional sample counter since last ZC
    int8_t   _polarity;            // current signal polarity (+1 / 0 / -1)

    // ── Bit accumulation ─────────────────────────────────────────────────────
    uint8_t  _bitBuffer[80];       // assembled bits (LSB order within byte)
    uint8_t  _bitCount;            // bits received so far

    // ── Output ───────────────────────────────────────────────────────────────
    LTCTimecode _timecode;
    bool        _frameReady;
    uint32_t    _totalBits;    // debug counter
    uint32_t    _zcResets;     // zero-crossing timeout count (signal dropouts)

    // ── Internal helpers ─────────────────────────────────────────────────────
    void  _handleCrossing(float intervalSamples);
    void  _pushBit(uint8_t bit);
    bool  _checkSyncWord() const;
    void  _decodeFrame();

    // Biphase-mark state: true = just saw a mid-bit crossing ("1" in progress),
    // waiting for the corresponding bit-boundary crossing.
    bool _expectMidBit;

    // Forward LTC sync word bits 64-79: 0 0 1 1 1 1 1 1 1 1 1 1 1 1 0 1
    // Stored with bit64 at position 0 (LSB) → 0xBFFC
    static constexpr uint16_t SYNC_WORD = 0xBFFC;
};
