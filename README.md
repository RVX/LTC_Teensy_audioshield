# LTC Teensy Audioshield — SMPTE LTC Timecode Reader

Reads **SMPTE LTC timecode** from an audio LINE IN signal and prints it to the serial monitor. Built for **Teensy 4.1 + Audio Shield (SGTL5000)**.

---

## Hardware

| Component | Notes |
|-----------|-------|
| **Teensy 4.1** | IMXRT1062, 600 MHz |
| **Teensy Audio Shield** (SGTL5000) | LINE IN = left 3.5 mm jack |

### Wiring

Connect your LTC source (e.g. Blackmagic HyperDeck BNC LTC OUT → BNC-to-3.5 mm cable) to the **LINE IN** jack on the Audio Shield (left channel). No termination resistor needed.

---

## Build

[PlatformIO](https://platformio.org/) with Arduino framework.

```bash
pio run                  # Build
pio run --target upload  # Upload to Teensy
pio device monitor       # Serial monitor (115200 baud)
```

## Serial Output

Each decoded LTC frame prints one line:

```
00:01:23:15
00:01:23:16
00:01:23:17
```

Format: `HH:MM:SS:FF` (or `HH:MM:SS;FF` for drop-frame).

## Configuration

All parameters in [`include/config.h`](include/config.h):

- **LTC_FRAMERATE** — 24, 25, or 30
- **AUDIO_LINEIN_LEVEL** — input sensitivity (0 = max, 15 = min)
- **LTC_ZC_THRESHOLD** — zero-crossing noise threshold
- Uncomment `DEBUG_LTC_BITS` to see raw decoded bits

## License

MIT
