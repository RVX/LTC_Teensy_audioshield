#pragma once

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include "config.h"

// ─────────────────────────────────────────────────────────────────────────────
// SSD1306 128×64 display layout
//
//  Row 0  (y=0,  h=16)  Timecode — HH:MM:SS size-2 (108px) + :FF size-1 (18px)
//  Row 1  (y=17, h=7)   Status: blinking ■ + LTC:OK/LTC:-- + input level bar
//  ─ divider (y=25) ─
//  Rows 2-4 (y=27..45)  Event log — 3 most recent events, newest at bottom
//  ─ divider (y=54) ─
//  Row 5  (y=56, h=8)   Stat line: mem / peak / zcErr / cpu
//
// ─────────────────────────────────────────────────────────────────────────────

#define OLED_WIDTH   128
#define OLED_HEIGHT   64
#define OLED_ADDR    0x3C
#define OLED_RESET   -1     // share Teensy reset

#define DISP_MSG_LINES  3   // number of scrolling message rows

class LTCDisplay {
public:
    LTCDisplay() : _display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET) {}

    // Call once in setup() — returns false if display not found
    bool begin() {
        if (!_display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) return false;
        // Codec (SGTL5000) is already configured before this is called.
        // Bump I2C to 1 MHz (Fast-mode Plus) — Teensy 4.1 supports it;
        // display-only traffic from here on so no need to drop back.
        Wire.setClock(1000000);
        _display.clearDisplay();
        _display.setTextWrap(false);
        _display.display();
        return true;
    }

    // ── Timecode row ─────────────────────────────────────────────────────────
    // Call every frame when LTC is valid.
    void setTimecode(uint8_t hh, uint8_t mm, uint8_t ss, uint8_t ff,
                     bool dropFrame, bool present)
    {
        _tcPresent = present;
        _hh = hh; _mm = mm; _ss = ss; _ff = ff; _dfFlag = dropFrame;
    }

    // ── Status bar ────────────────────────────────────────────────────────────
    // level: 0.0–1.0 peak amplitude
    void setStatus(bool ltcOK, float level, uint8_t memUsed, uint8_t memMax,
                   float cpu, uint32_t zcErr)
    {
        _ltcOK   = ltcOK;
        _level   = level;
        _memUsed = memUsed;
        _memMax  = memMax;
        _cpu     = cpu;
        _zcErr   = zcErr;
    }

    // ── Scrolling message log ─────────────────────────────────────────────────
    // Push a short string (max 21 chars) into the message area.
    void pushMessage(const char* msg) {
        // Shift lines up
        for (uint8_t i = 0; i < DISP_MSG_LINES - 1; ++i)
            memcpy(_msgBuf[i], _msgBuf[i + 1], sizeof(_msgBuf[0]));
        size_t len = strlen(msg);
        if (len >= sizeof(_msgBuf[0])) len = sizeof(_msgBuf[0]) - 1;
        memcpy(_msgBuf[DISP_MSG_LINES - 1], msg, len);
        _msgBuf[DISP_MSG_LINES - 1][len] = '\0';
    }

    // ── Render — call every loop() ────────────────────────────────────────────
    void update() {
        uint32_t now = millis();
        if (now - _lastDraw < DISPLAY_REFRESH_MS) return;
        _lastDraw = now;

        _display.clearDisplay();

        // ── Row 0: full timecode at size 2, 11px advance per char (121px total)
        //   HH:MM:SS drawn at y=0, separator+FF drawn at y=2 (2px lower)
        // ───────────────────────────────────────────────────────────────────
        _display.setTextColor(SSD1306_WHITE);
        _display.setTextSize(2);
        {
            char tc[12];
            if (_tcPresent) {
                snprintf(tc, sizeof(tc), "%02u:%02u:%02u%c%02u",
                         (unsigned)(_hh % 24), (unsigned)(_mm % 60),
                         (unsigned)(_ss % 60), _dfFlag ? ';' : ':',
                         (unsigned)(_ff % 100));
            } else {
                strcpy(tc, "--:--:--:--");
            }
            int16_t cx = 0;
            for (uint8_t i = 0; tc[i]; ++i) {
                // separator and frame digits sit 2px lower
                int16_t cy = (i >= 8) ? 2 : 0;
                _display.setCursor(cx, cy);
                _display.write((uint8_t)tc[i]);
                cx += 11;
            }
        }

        // ── Row 1: status indicator + label + level bar ─────────────────────
        // LTC active  : filled right-pointing triangle, blinks
        // LTC absent  : fixed solid square
        _display.setTextSize(1);
        if (_ltcOK) {
            _blink = !_blink;
            if (_blink)
                _display.fillTriangle(0, 17, 0, 23, 6, 20, SSD1306_WHITE);
        } else {
            _display.fillRect(0, 17, 7, 7, SSD1306_WHITE);
        }
        _display.setCursor(10, 17);
        _display.print(_ltcOK ? "LTC:OK" : "LTC:--");

        // Level bar: remaining width to right edge
        uint8_t barW = (uint8_t)(_level * 74.0f);
        if (barW > 74) barW = 74;
        _display.drawRect(52, 18, 76, 5, SSD1306_WHITE);
        if (barW > 0) _display.fillRect(53, 19, barW, 3, SSD1306_WHITE);

        // divider line
        _display.drawFastHLine(0, 25, OLED_WIDTH, SSD1306_WHITE);

        // ── Rows 2-4: event log — 3 most recent events, newest at bottom ───
        for (uint8_t i = 0; i < DISP_MSG_LINES; ++i) {
            _display.setCursor(0, 27 + i * 9);
            if (_msgBuf[i][0]) {
                _display.print('>');
                _display.print(_msgBuf[i]);
            }
        }

        // divider line
        _display.drawFastHLine(0, 54, OLED_WIDTH, SSD1306_WHITE);

        // ── Row 5: stat line ────────────────────────────────────────────────
        char stat[22];
        snprintf(stat, sizeof(stat), "m:%u/%u z:%lu c:%.0f%%",
                 _memUsed, _memMax, (unsigned long)_zcErr, _cpu);
        _display.setCursor(0, 56);
        _display.print(stat);

        _display.display();
    }

private:
    Adafruit_SSD1306 _display;

    uint8_t  _hh = 0, _mm = 0, _ss = 0, _ff = 0;
    bool     _dfFlag   = false;
    bool     _tcPresent = false;
    bool     _blink     = false;
    bool     _ltcOK    = false;
    float   _level                              = 0.0f;
    uint8_t _memUsed                            = 0;
    uint8_t _memMax                             = 0;
    float   _cpu                                = 0.0f;
    uint32_t _zcErr                             = 0;
    char    _msgBuf[DISP_MSG_LINES][22]         = {};
    uint32_t _lastDraw                          = 0;
};
