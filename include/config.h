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

// Audio peak — superseded by DEBUG_HEARTBEAT (lvl: field). Left for reference.
// #define DEBUG_AUDIO_LEVEL

// ─────────────────────────────────────────────────────────────────────────────
// Runtime Diagnostics
// ─────────────────────────────────────────────────────────────────────────────

// Milliseconds of silence before reporting [!!] LTC LOST
#define LTC_LOSS_TIMEOUT_MS    1000

// Interval (ms) between [STAT] heartbeat lines
#define HEARTBEAT_INTERVAL_MS  5000

// Comment out to disable the periodic [STAT] heartbeat line (Tier 2)
#define DEBUG_HEARTBEAT

// ─────────────────────────────────────────────────────────────────────────────
// SSD1306 Display
// ─────────────────────────────────────────────────────────────────────────────

// Display refresh interval — derived from LTC_FRAMERATE so one display update
// per LTC frame regardless of 24 / 25 / 29.97 / 30.
// At 1 MHz I2C the full framebuffer transfers in ~10 ms, leaving ample headroom.
#define DISPLAY_REFRESH_MS  (1000 / LTC_FRAMERATE)

// Comment out to disable the OLED display entirely
#define ENABLE_DISPLAY
