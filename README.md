# LTC Teensy Audioshield — SMPTE LTC Timecode Reader

Reads **SMPTE LTC timecode** from an audio LINE IN signal, displays it on an OLED, and streams diagnostics to Serial. Built for **Teensy 4.1 + Audio Shield (SGTL5000)**.

---

## Hardware

| Component | Notes |
|-----------|-------|
| **Teensy 4.1** | IMXRT1062, 600 MHz |
| **Teensy Audio Shield** (SGTL5000) | LINE IN = left 3.5 mm jack |
| **SSD1306 128×64 OLED** | I2C address `0x3C` |

### Wiring

Connect your LTC source (e.g. Blackmagic HyperDeck BNC LTC OUT → BNC-to-3.5 mm cable) to the **LINE IN** jack on the Audio Shield (left channel). No termination resistor needed.

The SSD1306 OLED shares the same I2C bus as the audio codec:

| OLED pin | Teensy 4.1 pin |
|----------|----------------|
| SDA      | 18             |
| SCL      | 19             |
| VCC      | 3.3 V          |
| GND      | GND            |

---

## Build

[PlatformIO](https://platformio.org/) with Arduino framework.

```bash
pio run                  # Build
pio run --target upload  # Upload to Teensy
pio device monitor       # Serial monitor (115200 baud)
```

---

## OLED Display

The 128×64 display is divided into five zones:

```
┌─────────────────────────────┐
│  00:01:23:15                │  ← timecode HH:MM:SS:FF (size 2)
│  ▶ LTC:OK  ████████░░░░░░░  │  ← status + input level bar
│─────────────────────────────│
│  > LTC acquired             │
│  > skip +1 @ 00:01:23:00    │  ← 3-line event log
│  > TC jump +3600f           │
│─────────────────────────────│
│  m:6(pk:8)/32 z:0 c:12%     │  ← memory / ZC errors / CPU
└─────────────────────────────┘
```

- Play triangle **▶** blinks each frame when LTC is present; solid **■** when no signal.
- The level bar reflects the real-time LINE IN peak amplitude.
- The event log scrolls — newest event at the bottom.
- Display is disabled automatically if the codec fails to initialise.

---

## Serial Output

Every decoded frame:

```
00:01:23:15
00:01:23:16
```

Format: `HH:MM:SS:FF` (`:` = non-drop, `;` = drop-frame).

**Tier 1 alerts** fire on state changes:

```
[LTC] acquired (30 fps)
[LTC] LOST (silence > 1000 ms)
[LTC] recovered
[LTC] frame skip +1 @ 00:01:23:00
[LTC] TC jump +3600 frames @ 00:01:25:00
[LTC] drop-frame flag changed → DF
```

**Tier 2 heartbeat** (every 5 s, disable with `DEBUG_HEARTBEAT`):

```
[STAT] LTC:OK lvl:42% skips:0 jumps:0 zcErr:0 mem:6(pk:8)/32 cpu:12%
```

---

## Configuration

All parameters in [`include/config.h`](include/config.h).

### Frame rate

```c
// 24 | 25 | 30  (29.97 drop-frame uses 30 — the DF bit is read from the stream)
#define LTC_FRAMERATE  30
```

Changing this single value automatically updates the bit-rate calculation, the BCD sanity check, and the display refresh interval.

### Key parameters

| Define | Default | Description |
|--------|---------|-------------|
| `LTC_FRAMERATE` | `30` | Nominal frame rate: 24, 25, or 30 |
| `AUDIO_LINEIN_LEVEL` | `0` | Input sensitivity — 0 (3.12 Vpp max) → 15 (0.24 Vpp min) |
| `LTC_ZC_THRESHOLD` | `0.001` | Zero-crossing noise floor (0.0–1.0) |
| `LTC_LOSS_TIMEOUT_MS` | `1000` | ms of silence before LTC LOST alert |
| `HEARTBEAT_INTERVAL_MS` | `5000` | ms between `[STAT]` lines |
| `DEBUG_HEARTBEAT` | defined | Comment out to suppress `[STAT]` output |
| `ENABLE_DISPLAY` | defined | Comment out to disable the OLED entirely |
| `DEBUG_LTC_BITS` | commented | Uncomment to print raw decoded bits |

---

## License

MIT
