/**
 * @file OledDisplay.cpp
 *
 * @project MrJ-ArduinoRailwayFX
 * @license MIT License — Copyright (c) 2026 HO44 PROJECT
 */

#include "oled/OledDisplay.h"

#ifdef MRJFX_OLED_ENABLED

  #include <stdio.h>
  #include <string.h>
  #include <Wire.h>

  #ifdef MRJFX_WIFI_ENABLED
    #include <WiFi.h>
  #endif

// ---------------------------------------------------------------------------
// Static members
// ---------------------------------------------------------------------------

char OledDisplay::_configName[32] = {};
char OledDisplay::_evtType[24] = {};
char OledDisplay::_evtId[24] = {};
int OledDisplay::_evtState = 0;
volatile bool OledDisplay::_hasEvent = false;
char OledDisplay::_logMsg[44] = {};
volatile bool OledDisplay::_hasLog = false;

// ---------------------------------------------------------------------------
// Global singleton — auto-registered with AceRoutine at construction.
// ---------------------------------------------------------------------------

OledDisplay oledDisplay;

// ---------------------------------------------------------------------------
// Construction / init
// ---------------------------------------------------------------------------

// U8G2 HW I2C constructor: pass U8X8_PIN_NONE for clock and data so that
// begin() uses the pre-initialized Wire instance without calling Wire.begin()
// again.  On arduino-esp32 v3.x, a second Wire.begin() call reinitialises the
// I2C peripheral and corrupts the bus.  Wire is started in MrJFX::init().
OledDisplay::OledDisplay()
    : _u8g2(U8G2_R0, U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE) {}

void OledDisplay::init() {
  if (!oledDisplay._begin()) {
    Serial.println(F("[OLED] no display found at 0x3C/0x3D — OLED disabled"));
    return;
  }
  Serial.println(F("[OLED] display found, task starting"));
  xTaskCreatePinnedToCore(_task, "oled", 8192, nullptr, 1, nullptr, 0); // Core 0
}

bool OledDisplay::_begin() {
  // SSD1306 needs up to ~100 ms to power up before it ACKs on I2C.
  // Retry up to 5 times (250 ms max) so an early boot doesn't miss the display.
  bool found = false;
  for (uint8_t attempt = 0; attempt < 5 && !found; attempt++) {
    if (attempt > 0) delay(50);
    Wire.beginTransmission(0x3C);
    found = (Wire.endTransmission() == 0);
    if (!found) {
      Wire.beginTransmission(0x3D);
      found = (Wire.endTransmission() == 0);
    }
  }
  if (!found)
    return false;

  _u8g2.begin();
  _u8g2.clearBuffer();
  _u8g2.setFont(u8g2_font_6x10_tr);
  _u8g2.drawStr(0, 12, "MrJ RailwayFX");
  _u8g2.setFont(u8g2_font_5x7_tr);
  _u8g2.drawStr(0, 22, MRJFX_FIRMWARE_VERSION);
  _u8g2.setFont(u8g2_font_6x10_tr);
  _u8g2.drawStr(0, 34, "Starting...");
  _u8g2.sendBuffer();
  return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void OledDisplay::showMessage(const char *line1, const char *line2) {
  oledDisplay._u8g2.clearBuffer();
  oledDisplay._u8g2.setFont(u8g2_font_6x10_tr);
  if (line1)
    oledDisplay._u8g2.drawStr(0, 12, line1);
  if (line2)
    oledDisplay._u8g2.drawStr(0, 26, line2);
  oledDisplay._u8g2.sendBuffer();
}

void OledDisplay::log(const char *msg) {
  strncpy(_logMsg, msg ? msg : "", sizeof(_logMsg) - 1);
  _logMsg[sizeof(_logMsg) - 1] = '\0';
  _hasLog = true;
}

void OledDisplay::setConfigName(const char *name) {
  strncpy(_configName, name ? name : "", sizeof(_configName) - 1);
  _configName[sizeof(_configName) - 1] = '\0';
}

void OledDisplay::log(const __FlashStringHelper *msg) {
  if (msg) {
    strncpy_P(_logMsg, (PGM_P)msg, sizeof(_logMsg) - 1);
    _logMsg[sizeof(_logMsg) - 1] = '\0';
    _hasLog = true;
  }
}

void OledDisplay::notify(const char *type, const char *id, int state) {
  strncpy(_evtType, type ? type : "", sizeof(_evtType) - 1);
  _evtType[sizeof(_evtType) - 1] = '\0';
  strncpy(_evtId, id ? id : "", sizeof(_evtId) - 1);
  _evtId[sizeof(_evtId) - 1] = '\0';
  _evtState = state;
  _hasEvent = true;
}

// ---------------------------------------------------------------------------
// Startup splash — steam train scrolling right→left  (#define OLED_SPLASH)
// ---------------------------------------------------------------------------

  #ifdef MRJFX_OLED_SPLASH_ENABLED

void OledDisplay::_drawTrain(int tx, int frame) {
  constexpr int W = 128;
  constexpr int yb = 54; // ground / top-of-rail y

  // Clipping helpers — U8g2 uses unsigned coords; negative x wraps incorrectly.
  auto box = [&](int x, int y, int w, int h) {
    if (x + w <= 0 || x >= W)
      return;
    if (x < 0) {
      w += x;
      x = 0;
    }
    _u8g2.drawBox(x, y, w, h);
  };
  auto hline = [&](int x, int y, int w) {
    if (x + w <= 0 || x >= W)
      return;
    if (x < 0) {
      w += x;
      x = 0;
    }
    _u8g2.drawHLine(x, y, w);
  };
  auto vline = [&](int x, int y, int len) {
    if (x < 0 || x >= W)
      return;
    _u8g2.drawVLine(x, y, len);
  };
  auto line = [&](int x0, int y0, int x1, int y1) {
    if ((x0 < 0 && x1 < 0) || (x0 >= W && x1 >= W))
      return;
    x0 = x0 < 0 ? 0 : (x0 >= W ? W - 1 : x0);
    x1 = x1 < 0 ? 0 : (x1 >= W ? W - 1 : x1);
    _u8g2.drawLine(x0, y0, x1, y1);
  };
  auto circle = [&](int cx, int cy, int r) {
    if (cx + r <= 0 || cx - r >= W)
      return;
    _u8g2.drawCircle(cx, cy, r);
  };
  auto disc = [&](int cx, int cy, int r) {
    if (cx + r <= 0 || cx - r >= W)
      return;
    _u8g2.drawDisc(cx, cy, r);
  };

  // Rails
  _u8g2.drawHLine(0, yb + 2, W);
  _u8g2.drawHLine(0, yb + 4, W);

  // Title + version
  _u8g2.setFont(u8g2_font_6x10_tr);
  _u8g2.setCursor(22, 11);
  _u8g2.print(F("MrJ Railway FX"));
  _u8g2.setFont(u8g2_font_5x7_tr);
  _u8g2.drawStr(22, 21, MRJFX_FIRMWARE_VERSION);

  // Cowcatcher (only when tx-7 >= 0 to avoid negative drawLine coords)
  if (tx >= 7 && tx < W) {
    _u8g2.drawLine(tx - 7, yb, tx, yb - 8);
    _u8g2.drawLine(tx - 4, yb, tx, yb - 5);
    _u8g2.drawHLine(tx - 7, yb, 7);
  }

  // Smokestack
  box(tx + 10, yb - 28, 5, 10);
  box(tx + 8, yb - 31, 9, 3);

  // Boiler
  box(tx, yb - 20, 50, 14);

  // Steam dome
  disc(tx + 24, yb - 21, 5);

  // Safety valve
  box(tx + 34, yb - 22, 2, 3);

  // Headlight
  box(tx, yb - 16, 3, 6);

  // Cab
  box(tx + 50, yb - 24, 14, 18);
  _u8g2.setDrawColor(0);
  box(tx + 52, yb - 22, 9, 8); // window (hollow)
  _u8g2.setDrawColor(1);

  // Tender
  box(tx + 67, yb - 18, 22, 12);
  box(tx + 68, yb - 21, 20, 4);
  hline(tx + 64, yb - 13, 3);

  // Wheels — spokes alternate + and × every kSplashSpokeFrames frames
  const bool cross = ((frame / kSplashSpokeFrames) % 2) == 1;

  auto drawWheel = [&](int cx, int cy, int r) {
    circle(cx, cy, r);
    if (cx + r > 0 && cx - r < W) {
      if (cross) {
        int d = (r * 7) / 10;
        line(cx - d, cy - d, cx + d, cy + d);
        line(cx - d, cy + d, cx + d, cy - d);
      } else {
        vline(cx, cy - r + 1, 2 * r - 2);
        hline(cx - r + 1, cy, 2 * r - 2);
      }
      disc(cx, cy, 2);
    }
  };

  drawWheel(tx + 10, yb - 4, 5); // pony
  drawWheel(tx + 24, yb - 3, 8); // drive 1
  drawWheel(tx + 43, yb - 3, 8); // drive 2
  drawWheel(tx + 71, yb - 4, 5); // tender 1
  drawWheel(tx + 81, yb - 4, 5); // tender 2

  // Connecting rod (2 px thick)
  hline(tx + 24, yb - 9, 19);
  hline(tx + 24, yb - 8, 19);

  // Smoke puffs
  const int sx = tx + 12;
  const int sy = yb - 32;
  const int pulse = (frame % 8 < 4) ? 0 : 1;
  circle(sx, sy, 2 + pulse);
  circle(sx + 5, sy - 5, 3);
  circle(sx + 11, sy - 10, 3 + pulse);
}

void OledDisplay::_drawSplash() {
  int frame = 0;
  for (int tx = 128; tx > -kSplashWidthPx; tx -= kSplashStepPx) {
    _u8g2.clearBuffer();
    _drawTrain(tx, frame++);
    _u8g2.sendBuffer();
    vTaskDelay(pdMS_TO_TICKS(kSplashDelayMs));
  }
  vTaskDelay(pdMS_TO_TICKS(500));
}

  #endif // MRJFX_OLED_SPLASH_ENABLED

// ---------------------------------------------------------------------------
// Coroutine body
// ---------------------------------------------------------------------------

void OledDisplay::_task(void *) {
  #ifdef MRJFX_OLED_SPLASH_ENABLED
  oledDisplay._drawSplash();
  #endif
  for (;;) {
    if (_hasEvent) {
      oledDisplay._drawEvent();
      _hasEvent = false;
      vTaskDelay(pdMS_TO_TICKS(OLED_EVENT_MS));
    } else if (_hasLog) {
      oledDisplay._drawLog();
      _hasLog = false;
      vTaskDelay(pdMS_TO_TICKS(2000));
    } else {
      oledDisplay._drawIdle();
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
}

// ---------------------------------------------------------------------------
// Screen: idle
// ---------------------------------------------------------------------------

void OledDisplay::_drawIdle() {
  _u8g2.clearBuffer();

  #if OLED_HEIGHT >= 64
  // ── Line 1 (y=10): config name or project name ────────────────────────────
  _u8g2.setFont(u8g2_font_6x10_tr);
  _u8g2.drawStr(0, 10, _configName[0] ? _configName : "MrJ RailwayFX");
  _u8g2.drawHLine(0, 13, 128);

  // ── Line 2 (y=25): IP address + WiFi signal bars (right-aligned) ──────────
  char ip[20] = "No WiFi";
    #ifdef MRJFX_WIFI_ENABLED
  if (WiFi.status() == WL_CONNECTED) {
    strncpy(ip, WiFi.localIP().toString().c_str(), sizeof(ip) - 1);
  } else {
    String apStr = WiFi.softAPIP().toString();
    strncpy(ip, apStr != "0.0.0.0" ? apStr.c_str() : "Connecting...", sizeof(ip) - 1);
  }
    #endif
  _u8g2.drawStr(0, 25, ip);

    #ifdef MRJFX_WIFI_ENABLED
  // WiFi bars: 4 bars right-aligned, anchored at bottom y=24
  {
    int rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -100;
    int bars = (rssi >= -60) ? 4 : (rssi >= -70) ? 3 : (rssi >= -80) ? 2 : 1;
    for (int b = 0; b < 4; b++) {
      uint8_t bh = (uint8_t)(3 + b * 2); // heights: 3, 5, 7, 9
      uint8_t bx = (uint8_t)(107 + b * 5);
      uint8_t by = (uint8_t)(24 - bh + 1);
      if (b < bars) {
        _u8g2.drawBox(bx, by, 4, bh);
      } else {
        _u8g2.drawFrame(bx, by, 4, bh);
      }
    }
  }
    #endif

  // ── Line 3 (y=39): uptime ─────────────────────────────────────────────────
  char uptime[20];
  {
    unsigned long s = millis() / 1000UL;
    snprintf(uptime, sizeof(uptime), "up %02lu:%02lu:%02lu",
             s / 3600UL, (s % 3600UL) / 60UL, s % 60UL);
  }
  _u8g2.drawStr(0, 39, uptime);

  // ── Line 4 (y=52): active features — compile-time constant ────────────────
  {
    static const char kFeats[] =
    #ifdef MRJFX_WIFI_ENABLED
      "WiFi "
    #endif
    #ifdef MRJFX_CONFIG_ENABLED
      "CFG "
    #endif
    #ifdef MRJFX_I2C_DEVICES_ENABLED
      "I2C "
    #endif
    #ifdef MRJFX_SPI_CARDS_ENABLED
      "SPI "
    #endif
    #ifdef MRJFX_DCC_ENABLED
      "DCC"
    #endif
      "";
    _u8g2.setFont(u8g2_font_5x7_tr);
    _u8g2.drawStr(0, 52, kFeats);
  }

  #else // 128×32
  _u8g2.setFont(u8g2_font_6x10_tr);
  _u8g2.drawStr(0, 8, "MrJ FX");

  char ip[20] = "No WiFi";
    #ifdef MRJFX_WIFI_ENABLED
  if (WiFi.status() == WL_CONNECTED) {
    strncpy(ip, WiFi.localIP().toString().c_str(), sizeof(ip) - 1);
  } else {
    String apStr = WiFi.softAPIP().toString();
    strncpy(ip, apStr != "0.0.0.0" ? apStr.c_str() : "Connecting...", sizeof(ip) - 1);
  }
    #endif
  _u8g2.drawStr(0, 20, ip);

  char uptime[18];
  unsigned long s = millis() / 1000UL;
  snprintf(uptime, sizeof(uptime), "%02lu:%02lu:%02lu",
           s / 3600UL, (s % 3600UL) / 60UL, s % 60UL);
  _u8g2.drawStr(0, 30, uptime);
  #endif

  _u8g2.sendBuffer();
}

// ---------------------------------------------------------------------------
// Screen: event (device state change)
// ---------------------------------------------------------------------------

void OledDisplay::_drawEvent() {
  _u8g2.clearBuffer();

  #if OLED_HEIGHT >= 64
  // Icon: 32×32 at (0, 16) — vertically centred on 64 px
  _drawIcon(_evtType, 0, 16);

  // Vertical separator
  _u8g2.drawVLine(34, 0, 64);

  // Type name (up to 13 chars × 5 px = 65 px)
  _u8g2.setFont(u8g2_font_5x7_tr);
  char tname[14];
  strncpy(tname, _evtType, sizeof(tname) - 1);
  tname[sizeof(tname) - 1] = '\0';
  _u8g2.drawStr(37, 10, tname);

  // Device id
  char did[14];
  strncpy(did, _evtId, sizeof(did) - 1);
  did[sizeof(did) - 1] = '\0';
  _u8g2.drawStr(37, 22, did);

  // State label — larger font
  _u8g2.setFont(u8g2_font_8x13B_tr);
  _u8g2.drawStr(37, 50, _stateName(_evtType, _evtState));

  #else // 128×32 — no icon, text only
  _u8g2.setFont(u8g2_font_5x7_tr);
  char tname[22];
  strncpy(tname, _evtType, sizeof(tname) - 1);
  tname[sizeof(tname) - 1] = '\0';
  _u8g2.drawStr(0, 7, tname);

  char did[22];
  strncpy(did, _evtId, sizeof(did) - 1);
  did[sizeof(did) - 1] = '\0';
  _u8g2.drawStr(0, 17, did);

  _u8g2.setFont(u8g2_font_7x13B_tr);
  _u8g2.drawStr(0, 30, _stateName(_evtType, _evtState));
  #endif

  _u8g2.sendBuffer();
}

// ---------------------------------------------------------------------------
// Screen: log message
// ---------------------------------------------------------------------------

void OledDisplay::_drawLog() {
  _u8g2.clearBuffer();
  _u8g2.setFont(u8g2_font_6x10_tr);

  #if OLED_HEIGHT >= 64
  _u8g2.drawStr(0, 10, "LOG");
  _u8g2.drawHLine(0, 13, 128);
  // Word-wrap: two lines of ~21 chars each
  char line1[22] = {};
  char line2[22] = {};
  size_t len = strlen(_logMsg);
  if (len <= 21) {
    strncpy(line1, _logMsg, 21);
  } else {
    strncpy(line1, _logMsg, 21);
    strncpy(line2, _logMsg + 21, 21);
  }
  _u8g2.drawStr(0, 28, line1);
  _u8g2.drawStr(0, 42, line2);
  #else // 128×32
  _u8g2.drawStr(0, 8, "LOG");
  // One truncated line
  char line[22] = {};
  strncpy(line, _logMsg, 21);
  _u8g2.drawStr(0, 24, line);
  #endif

  _u8g2.sendBuffer();
}

// ---------------------------------------------------------------------------
// State label
// ---------------------------------------------------------------------------

const char *OledDisplay::_stateName(const char *type, int state) {
  if (strcmp(type, "MrJDBBlocSignal") == 0) {
    switch (state) {
    case 1:
      return "HP0";
    case 2:
      return "HP1";
    }
  } else if (strcmp(type, "MrJDBEntrySignal") == 0) {
    switch (state) {
    case 1:
      return "HP0";
    case 2:
      return "HP1";
    case 3:
      return "HP2";
    }
  } else if (strcmp(type, "MrJDBExitSignal") == 0) {
    switch (state) {
    case 1:
      return "HP00";
    case 2:
      return "HP1";
    case 3:
      return "HP2";
    case 4:
      return "HP0+SH1";
    }
  } else if (strcmp(type, "TrafficLight3ph") == 0 ||
             strcmp(type, "TrafficLight4ph") == 0) {
    switch (state) {
    case 0:
      return "STOP";
    case 1:
      return "GO";
    case 2:
      return "GO";
    case 3:
      return "FLASH";
    }
  }
  return state > 0 ? "ON" : "OFF";
}

// ---------------------------------------------------------------------------
// Icon renderer — 32×32 box with top-left at (ox, oy)
// SVG viewBox is 24×24 → scale factor 32/24 = 4/3.
// Coordinates below are pre-computed: round(svg_coord * 4 / 3).
// ---------------------------------------------------------------------------

void OledDisplay::_drawIcon(const char *type, uint8_t ox, uint8_t oy) {

  // ── DB signals ───────────────────────────────────────────────────────────
  // SVG: rect x=8 y=1 w=8 h=20 rx=2  →  x=11 y=1 w=11 h=27 rx=3
  //      mast: line (12,22)-(12,21)   →  (16,29)-(16,28)
  if (strcmp(type, "MrJDBBlocSignal") == 0 ||
      strcmp(type, "MrJDBEntrySignal") == 0 ||
      strcmp(type, "MrJDBExitSignal") == 0) {
    _u8g2.drawRFrame(ox + 11, oy + 1, 11, 27, 3);
    _u8g2.drawVLine(ox + 16, oy + 28, 3);

    if (strcmp(type, "MrJDBBlocSignal") == 0) {
      // cx=10,cy=15 r=1.5 → cx=13,cy=20 r=2
      // cx=14,cy=15 r=1.5 → cx=19,cy=20 r=2
      _u8g2.drawDisc(ox + 13, oy + 20, 2);
      _u8g2.drawDisc(ox + 19, oy + 20, 2);
    } else if (strcmp(type, "MrJDBEntrySignal") == 0) {
      // cx=14,cy=6  r=1.5 → cx=19,cy=8  r=2  (top-right)
      // cx=10,cy=15 r=1.5 → cx=13,cy=20 r=2  (bottom-left)
      // cx=14,cy=15 r=1.5 → cx=19,cy=20 r=2  (bottom-right)
      _u8g2.drawDisc(ox + 19, oy + 8, 2);
      _u8g2.drawDisc(ox + 13, oy + 20, 2);
      _u8g2.drawDisc(ox + 19, oy + 20, 2);
    } else {
      // Exit — 5 rows
      // cx=10,cy=4.5  r=1.5 → cx=13,cy=6  r=2
      // cx=10,cy=8    r=1.5 → cx=13,cy=11 r=2
      // cx=14,cy=8    r=1.5 → cx=19,cy=11 r=2
      // cx=14,cy=11.5 r=1.1 → cx=19,cy=15 r=1
      // cx=10,cy=15   r=1.1 → cx=13,cy=20 r=1
      // cx=10,cy=18.5 r=1.5 → cx=13,cy=25 r=2
      _u8g2.drawDisc(ox + 13, oy + 6, 2);
      _u8g2.drawDisc(ox + 13, oy + 11, 2);
      _u8g2.drawDisc(ox + 19, oy + 11, 2);
      _u8g2.drawDisc(ox + 19, oy + 15, 1);
      _u8g2.drawDisc(ox + 13, oy + 20, 1);
      _u8g2.drawDisc(ox + 13, oy + 25, 2);
    }
    return;
  }

  // ── Traffic lights ───────────────────────────────────────────────────────
  // SVG 3ph: rect x=7 y=2 w=10 h=17 rx=2 → x=9 y=3 w=13 h=23 rx=3
  //          circles cy=6,11,16 r=2       → cy=8,15,21 r=3
  //          mast line y=19-22            → y=25-29
  if (strcmp(type, "TrafficLight3ph") == 0) {
    _u8g2.drawRFrame(ox + 9, oy + 3, 13, 23, 3);
    _u8g2.drawVLine(ox + 15, oy + 26, 4);
    _u8g2.drawDisc(ox + 15, oy + 8, 3);
    _u8g2.drawDisc(ox + 15, oy + 15, 3);
    _u8g2.drawDisc(ox + 15, oy + 21, 3);
    return;
  }
  // SVG 4ph: rect x=7 y=1 w=10 h=20 rx=2 → x=9 y=1 w=13 h=27 rx=3
  //          circles cy=5,10,15,20 r=1.7  → cy=7,13,20,27 r=2
  if (strcmp(type, "TrafficLight4ph") == 0) {
    _u8g2.drawRFrame(ox + 9, oy + 1, 13, 27, 3);
    _u8g2.drawDisc(ox + 15, oy + 7, 2);
    _u8g2.drawDisc(ox + 15, oy + 13, 2);
    _u8g2.drawDisc(ox + 15, oy + 20, 2);
    _u8g2.drawDisc(ox + 15, oy + 27, 2);
    return;
  }

  // ── Led ──────────────────────────────────────────────────────────────────
  // SVG: circle cx=12 cy=11 r=4 → cx=16 cy=15 r=5
  //      8 rays, base line y=21, stem y=15-21
  if (strcmp(type, "Led") == 0) {
    _u8g2.drawCircle(ox + 16, oy + 15, 5);
    _u8g2.drawLine(ox + 16, oy + 1, ox + 16, oy + 3);   // top
    _u8g2.drawLine(ox + 16, oy + 24, ox + 16, oy + 27); // bottom
    _u8g2.drawLine(ox + 1, oy + 15, ox + 4, oy + 15);   // left
    _u8g2.drawLine(ox + 29, oy + 15, ox + 31, oy + 15); // right
    _u8g2.drawLine(ox + 7, oy + 6, ox + 9, oy + 8);     // top-left
    _u8g2.drawLine(ox + 20, oy + 21, ox + 22, oy + 23); // bottom-right
    _u8g2.drawLine(ox + 7, oy + 23, ox + 9, oy + 21);   // bottom-left
    _u8g2.drawLine(ox + 20, oy + 9, ox + 22, oy + 7);   // top-right
    _u8g2.drawHLine(ox + 12, oy + 28, 8);               // base
    _u8g2.drawVLine(ox + 16, oy + 20, 8);               // stem
    return;
  }

  // ── Beacon ───────────────────────────────────────────────────────────────
  // SVG: circle cx=12 cy=10 r=3.5 → cx=16 cy=13 r=5
  //      8 rays, base line x=8-16 y=21
  if (strcmp(type, "Beacon") == 0) {
    _u8g2.drawCircle(ox + 16, oy + 13, 5);
    _u8g2.drawLine(ox + 16, oy + 1, ox + 16, oy + 4);   // top
    _u8g2.drawLine(ox + 16, oy + 21, ox + 16, oy + 24); // bottom
    _u8g2.drawLine(ox + 1, oy + 13, ox + 4, oy + 13);   // left
    _u8g2.drawLine(ox + 29, oy + 13, ox + 31, oy + 13); // right
    _u8g2.drawLine(ox + 6, oy + 5, ox + 8, oy + 8);     // top-left
    _u8g2.drawLine(ox + 22, oy + 22, ox + 25, oy + 24); // bottom-right
    _u8g2.drawLine(ox + 6, oy + 22, ox + 8, oy + 19);   // bottom-left
    _u8g2.drawLine(ox + 22, oy + 5, ox + 25, oy + 2);   // top-right
    _u8g2.drawHLine(ox + 11, oy + 28, 10);              // base
    return;
  }

  // ── DoubleBeacon ─────────────────────────────────────────────────────────
  // SVG: two beacons at cx=7,cy=9 and cx=17,cy=9, r=2.5
  //      base line x=5-19 y=20
  if (strcmp(type, "DoubleBeacon") == 0) {
    // Left beacon cx=7,cy=9 r=2.5 → cx=9,cy=12 r=3
    _u8g2.drawCircle(ox + 9, oy + 12, 3);
    _u8g2.drawLine(ox + 9, oy + 3, ox + 9, oy + 5);
    _u8g2.drawLine(ox + 9, oy + 19, ox + 9, oy + 21);
    _u8g2.drawLine(ox + 1, oy + 12, ox + 3, oy + 12);
    _u8g2.drawLine(ox + 15, oy + 12, ox + 13, oy + 12);
    _u8g2.drawLine(ox + 5, oy + 7, ox + 7, oy + 9);
    _u8g2.drawLine(ox + 5, oy + 17, ox + 7, oy + 15);
    _u8g2.drawLine(ox + 11, oy + 7, ox + 13, oy + 5);
    _u8g2.drawLine(ox + 11, oy + 17, ox + 13, oy + 19);
    // Right beacon cx=17,cy=9 r=2.5 → cx=23,cy=12 r=3
    _u8g2.drawCircle(ox + 23, oy + 12, 3);
    _u8g2.drawLine(ox + 23, oy + 3, ox + 23, oy + 5);
    _u8g2.drawLine(ox + 23, oy + 19, ox + 23, oy + 21);
    _u8g2.drawLine(ox + 17, oy + 12, ox + 19, oy + 12);
    _u8g2.drawLine(ox + 29, oy + 12, ox + 27, oy + 12);
    _u8g2.drawLine(ox + 19, oy + 7, ox + 21, oy + 9);
    _u8g2.drawLine(ox + 19, oy + 17, ox + 21, oy + 15);
    _u8g2.drawLine(ox + 25, oy + 7, ox + 27, oy + 5);
    _u8g2.drawLine(ox + 25, oy + 17, ox + 27, oy + 19);
    // Base line x=5-19 y=20 → (7,27)-(25,27)
    _u8g2.drawHLine(ox + 7, oy + 27, 18);
    return;
  }

  // ── Gas Lamp ─────────────────────────────────────────────────────────────
  // SVG: mast line y=22-12 → (16,29)-(16,16)
  //      3 arms (bezier ≈ line) to cx=7,12,17 cy=7.5 → cx=9,16,23 cy=10
  //      3 filled circles r=1.5 → r=2
  //      base rect x=10 y=22 w=4 → (13,29) w=5
  if (strcmp(type, "GasLamp") == 0) {
    _u8g2.drawVLine(ox + 16, oy + 16, 13);
    _u8g2.drawLine(ox + 16, oy + 16, ox + 9, oy + 10);
    _u8g2.drawLine(ox + 16, oy + 16, ox + 16, oy + 10);
    _u8g2.drawLine(ox + 16, oy + 16, ox + 23, oy + 10);
    _u8g2.drawDisc(ox + 9, oy + 10, 2);
    _u8g2.drawDisc(ox + 16, oy + 10, 2);
    _u8g2.drawDisc(ox + 23, oy + 10, 2);
    _u8g2.drawHLine(ox + 13, oy + 29, 6);
    return;
  }

  // ── Electric Lamp ────────────────────────────────────────────────────────
  // SVG: bulb path (≈ circle r=6 cx=12 cy=9) + flat bottom at y=17-19
  //      connectors: M10 21h4 / M11 21v-2 M13 21v-2
  if (strcmp(type, "ElectricLamp") == 0) {
    _u8g2.drawCircle(ox + 16, oy + 12, 8);
    _u8g2.drawHLine(ox + 13, oy + 21, 7); // flat bottom
    _u8g2.drawHLine(ox + 13, oy + 23, 7);
    _u8g2.drawVLine(ox + 15, oy + 21, 3); // left connector
    _u8g2.drawVLine(ox + 17, oy + 21, 3); // right connector
    return;
  }

  // ── Defect Lamp ──────────────────────────────────────────────────────────
  // Same as Electric Lamp + X cross inside
  if (strcmp(type, "DefectLamp") == 0) {
    _u8g2.drawCircle(ox + 16, oy + 12, 8);
    _u8g2.drawHLine(ox + 13, oy + 21, 7);
    _u8g2.drawHLine(ox + 13, oy + 23, 7);
    _u8g2.drawVLine(ox + 15, oy + 21, 3);
    _u8g2.drawVLine(ox + 17, oy + 21, 3);
    // X: SVG line (9.5,7.5)-(14.5,14.5) and (14.5,7.5)-(9.5,14.5)
    _u8g2.drawLine(ox + 13, oy + 10, ox + 19, oy + 19);
    _u8g2.drawLine(ox + 19, oy + 10, ox + 13, oy + 19);
    return;
  }

  // ── Camp Fire ────────────────────────────────────────────────────────────
  // SVG: flame path (teardrop) + ground line y=22 + sticks
  if (strcmp(type, "CampFire") == 0) {
    // Flame (approximated as elongated teardrop)
    _u8g2.drawLine(ox + 16, oy + 3, ox + 10, oy + 13); // left side
    _u8g2.drawLine(ox + 16, oy + 3, ox + 22, oy + 13); // right side
    _u8g2.drawEllipse(ox + 16, oy + 17, 6, 5, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
    // Ground line y=22 → 29
    _u8g2.drawHLine(ox + 7, oy + 29, 18);
    // Left stick
    _u8g2.drawLine(ox + 8, oy + 24, ox + 13, oy + 29);
    // Right stick
    _u8g2.drawLine(ox + 24, oy + 24, ox + 19, oy + 29);
    return;
  }

  // ── Torch ────────────────────────────────────────────────────────────────
  // SVG: diagonal handle (6,22)-(14,13) + (12,14)-(16,12)
  //      flame path M13 13 Q10 9 13 5 Q15 8 16 6 Q18 10 16 13Z (≈ teardrop)
  if (strcmp(type, "Torch") == 0) {
    _u8g2.drawLine(ox + 8, oy + 29, ox + 19, oy + 17);  // main handle
    _u8g2.drawLine(ox + 16, oy + 19, ox + 21, oy + 16); // secondary handle
    // Flame (≈ oval at tip of torch)
    _u8g2.drawCircle(ox + 20, oy + 11, 4);
    _u8g2.drawDisc(ox + 20, oy + 13, 2);
    return;
  }

  // ── Storm ────────────────────────────────────────────────────────────────
  // SVG: cloud path (arc/oval top) + lightning polyline 12,11→10,16→14,16→12,21
  if (strcmp(type, "Storm") == 0) {
    // Simplified cloud: two overlapping circles
    _u8g2.drawCircle(ox + 12, oy + 9, 5);
    _u8g2.drawCircle(ox + 20, oy + 9, 4);
    _u8g2.drawHLine(ox + 7, oy + 13, 17);
    // Lightning bolt: (16,15)→(13,21)→(19,21)→(16,28)
    _u8g2.drawLine(ox + 16, oy + 15, ox + 13, oy + 21);
    _u8g2.drawLine(ox + 13, oy + 21, ox + 19, oy + 21);
    _u8g2.drawLine(ox + 19, oy + 21, ox + 16, oy + 28);
    return;
  }

  // ── NeonSign ─────────────────────────────────────────────────────────────
  // SVG: outer rect x=2 y=4 w=20 h=16 rx=2 → x=3 y=5 w=27 h=21 rx=3
  //      inner rect x=5 y=7 w=14 h=10 rx=1 → x=7 y=9 w=19 h=13 rx=1
  //      letters inside (too complex at 32px → horizontal lines)
  if (strcmp(type, "NeonSign") == 0) {
    _u8g2.drawRFrame(ox + 3, oy + 5, 27, 21, 3);
    _u8g2.drawRFrame(ox + 7, oy + 9, 19, 13, 1);
    _u8g2.drawHLine(ox + 9, oy + 13, 7);
    _u8g2.drawHLine(ox + 17, oy + 13, 7);
    return;
  }

  // ── Oil Lamp ─────────────────────────────────────────────────────────────
  // SVG: foot rect x=10 y=17 w=4 → (13,23) w=5
  //      reservoir: lens shape, wide oval centred ~(12,14), width ~16, height ~5
  //      spout/bec on left at ~(4,14) curving up to (4,7)
  //      flame at ~(4,8) tiny disc
  //      handle circle cx=21,cy=13 r=1.5 → cx=28,cy=17 r=2
  if (strcmp(type, "OilLamp") == 0) {
    _u8g2.drawEllipse(ox + 16, oy + 19, 11, 4, U8G2_DRAW_ALL);
    _u8g2.drawRFrame(ox + 13, oy + 23, 6, 3, 1);      // foot
    _u8g2.drawLine(ox + 5, oy + 19, ox + 3, oy + 13); // spout
    _u8g2.drawDisc(ox + 3, oy + 11, 2);               // flame
    _u8g2.drawCircle(ox + 28, oy + 17, 2);            // handle
    return;
  }

  // ── Signal Flare ─────────────────────────────────────────────────────────
  // SVG: star circle cx=12 cy=5 r=1.5 → cx=16 cy=7 r=2
  //      6 short rays around it
  //      vertical pole (12,6.5)-(12,17) → (16,9)-(16,23)
  //      base rect x=10 y=17 w=4 h=5 → x=13 y=23 w=5 h=7
  if (strcmp(type, "SignalFlare") == 0) {
    _u8g2.drawDisc(ox + 16, oy + 7, 2);
    _u8g2.drawLine(ox + 16, oy + 1, ox + 16, oy + 4);  // top ray
    _u8g2.drawLine(ox + 21, oy + 3, ox + 19, oy + 5);  // top-right
    _u8g2.drawLine(ox + 23, oy + 7, ox + 21, oy + 7);  // right
    _u8g2.drawLine(ox + 21, oy + 11, ox + 19, oy + 9); // bottom-right
    _u8g2.drawLine(ox + 11, oy + 3, ox + 13, oy + 5);  // top-left
    _u8g2.drawLine(ox + 9, oy + 7, ox + 11, oy + 7);   // left
    _u8g2.drawLine(ox + 11, oy + 11, ox + 13, oy + 9); // bottom-left
    _u8g2.drawVLine(ox + 16, oy + 9, 13);              // pole
    _u8g2.drawRFrame(ox + 13, oy + 23, 6, 7, 1);       // base
    return;
  }

  // ── Solder Lamp ──────────────────────────────────────────────────────────
  // SVG: arch body M4,21 Q4,3 12,3 Q20,3 20,21 Z → tall rounded-top rect
  //      window rect x=6 y=10 w=12 h=5 rx=1 → x=8 y=13 w=16 h=7 rx=1
  if (strcmp(type, "SolderLamp") == 0) {
    _u8g2.drawRFrame(ox + 5, oy + 4, 22, 24, 8); // arch body
    _u8g2.drawRFrame(ox + 8, oy + 13, 16, 7, 1); // window
    return;
  }

  // ── Railway Crossing Lights ───────────────────────────────────────────────
  // SVG: vertical line (12,2)-(12,22) → (16,3)-(16,29)
  //      diagonal (3,7)-(21,17) → (4,9)-(28,23)
  //      diagonal (21,7)-(3,17) → (28,9)-(4,23)
  //      filled circles at (3,7) and (21,17) r=2.5 → (4,9) and (28,23) r=3
  if (strcmp(type, "RailwayCrossingLights") == 0) {
    _u8g2.drawVLine(ox + 16, oy + 3, 26);
    _u8g2.drawLine(ox + 4, oy + 9, ox + 28, oy + 23);
    _u8g2.drawLine(ox + 28, oy + 9, ox + 4, oy + 23);
    _u8g2.drawDisc(ox + 4, oy + 9, 3);
    _u8g2.drawDisc(ox + 28, oy + 23, 3);
    return;
  }

  // ── Train Head Lamp ───────────────────────────────────────────────────────
  // SVG: rect x=3 y=7 w=18 h=12 rx=3 → x=4 y=9 w=24 h=16 rx=4
  //      circle cx=8.5,cy=13 r=2.5 → cx=11,cy=17 r=3
  //      circle cx=15.5,cy=13 r=2.5 → cx=21,cy=17 r=3
  //      rail lines, legs
  if (strcmp(type, "TrainHeadLamp") == 0) {
    _u8g2.drawRFrame(ox + 4, oy + 9, 24, 16, 4);
    _u8g2.drawDisc(ox + 11, oy + 17, 3);
    _u8g2.drawDisc(ox + 21, oy + 17, 3);
    _u8g2.drawLine(ox + 4, oy + 13, ox + 1, oy + 13);
    _u8g2.drawLine(ox + 4, oy + 21, ox + 1, oy + 21);
    _u8g2.drawLine(ox + 11, oy + 25, ox + 9, oy + 29);
    _u8g2.drawLine(ox + 21, oy + 25, ox + 23, oy + 29);
    return;
  }

  // ── Turn Signal ───────────────────────────────────────────────────────────
  // SVG: path M4 12L13 3V8H19V16H13V21Z (right-pointing arrow)
  //      scaled: (5,16)→(17,4)→(17,11)→(25,11)→(25,21)→(17,21)→(17,28)→(5,16)
  if (strcmp(type, "TurnSignal") == 0) {
    _u8g2.drawLine(ox + 5, oy + 16, ox + 17, oy + 4);
    _u8g2.drawLine(ox + 17, oy + 4, ox + 17, oy + 11);
    _u8g2.drawLine(ox + 17, oy + 11, ox + 25, oy + 11);
    _u8g2.drawLine(ox + 25, oy + 11, ox + 25, oy + 21);
    _u8g2.drawLine(ox + 25, oy + 21, ox + 17, oy + 21);
    _u8g2.drawLine(ox + 17, oy + 21, ox + 17, oy + 28);
    _u8g2.drawLine(ox + 17, oy + 28, ox + 5, oy + 16);
    return;
  }

  // ── Default: question-mark circle ────────────────────────────────────────
  _u8g2.setFont(u8g2_font_6x10_tr);
  _u8g2.drawCircle(ox + 16, oy + 16, 12);
  _u8g2.drawStr(ox + 13, oy + 21, "?");
}

#endif // MRJFX_OLED_ENABLED
