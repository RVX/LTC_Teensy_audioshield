#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Audio Shield (SGTL5000) Configuration
// ─────────────────────────────────────────────────────────────────────────────

// Input source: AUDIO_INPUT_LINEIN or AUDIO_INPUT_MIC
#define AUDIO_INPUT_SOURCE    AUDIO_INPUT_LINEIN

// Line-in level: 0 (3.12 Vpp) to 15 (0.24 Vpp), default 5 (~1.33 Vpp)
// Lowered to 0 for maximum sensitivity.
#define AUDIO_LINEIN_LEVEL    0

// Headphone / line-out volume: 0.0 – 1.0
#define AUDIO_VOLUME          0.5f

// Number of AudioMemory blocks (each block = AUDIO_BLOCK_SAMPLES * 2 bytes)
// Increase if you see audio glitches in the serial monitor.
#define AUDIO_MEMORY_BLOCKS   32

// ─────────────────────────────────────────────────────────────────────────────
// LTC Decoder Configuration
// ─────────────────────────────────────────────────────────────────────────────

// Expected LTC frame rate: 24 | 25 | 2997 (29.97 drop) | 30
#define LTC_FRAMERATE         30

// Nominal samples per LTC bit at the configured frame rate.
// 44117.64706f is the Teensy audio engine's exact sample rate (AUDIO_SAMPLE_RATE_EXACT).
// LTC frame = 80 bits; samples per frame = sample_rate / LTC_FRAMERATE
// samples per bit = samples per frame / 80
#define LTC_SAMPLES_PER_BIT   (44117.64706f / LTC_FRAMERATE / 80.0f)

// Zero-crossing detection threshold (0.0 – 1.0, relative to full-scale)
// Raise if noise triggers false crossings; lower if signal is weak.
#define LTC_ZC_THRESHOLD      0.001f

// ─────────────────────────────────────────────────────────────────────────────
// Debug / Serial
// ─────────────────────────────────────────────────────────────────────────────

#define SERIAL_BAUD_RATE      115200

// Uncomment to print raw decoded bits to Serial
// #define DEBUG_LTC_BITS

// Uncomment to print audio peak levels to Serial
// #define DEBUG_AUDIO_LEVEL
