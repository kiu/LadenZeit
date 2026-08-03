#include "lz_oled.h"

#include "lz_config.h"

#include <SPI.h>
#include <U8g2lib.h>
#include <Preferences.h>

#include "LadenZeit.h"
#include "lz_places.h"
#include "lz_network.h"
#include "lz_time.h"

#define OLED_WIDTH 256
#define OLED_HEIGHT 64

Preferences oledPref;
const char* OLED_PREF = "oled";
const char* OLED_PREF_FLIP = "flip";

U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI u8g2(U8G2_R0, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RES);

struct OledRenderIcon {
  const uint8_t* font;
  uint8_t icon;
  const char* name;
};

OledRenderIcon oledRenderIcon1Check = { u8g2_font_open_iconic_check_1x_t, 0x40, "" };
OledRenderIcon oledRenderIcon1X = { u8g2_font_open_iconic_check_1x_t, 0x44, "" };
OledRenderIcon oledRenderIcon1Back = { u8g2_font_open_iconic_embedded_1x_t, 0x44, "" };

OledRenderIcon oledRenderIcon1Locked = { u8g2_font_open_iconic_thing_1x_t, 0x4F, "" };
OledRenderIcon oledRenderIcon1Unlocked = { u8g2_font_open_iconic_thing_1x_t, 0x44, "" };

OledRenderIcon oledRenderIcon1Pause = { u8g2_font_open_iconic_play_1x_t, 0x44, "" };
OledRenderIcon oledRenderIcon1Start = { u8g2_font_open_iconic_play_1x_t, 0x45, "" };

OledRenderIcon oledRenderIcon4Menu = { u8g2_font_open_iconic_embedded_4x_t, 0x42, "Menu" };
OledRenderIcon oledRenderIcon4MenuItems[] = {
  { u8g2_font_open_iconic_embedded_4x_t, 0x44, "Back" },
  { u8g2_font_open_iconic_www_4x_t, 0x47, "Places" },
  { u8g2_font_open_iconic_embedded_4x_t, 0x50, "Wifi" },
  { u8g2_font_open_iconic_embedded_4x_t, 0x4F, "Refresh" },
  { u8g2_font_open_iconic_www_4x_t, 0x53, "Handed" },
  { u8g2_font_open_iconic_embedded_4x_t, 0x47, "Factory" },
};

static_assert(sizeof(oledRenderIcon4MenuItems) / sizeof(oledRenderIcon4MenuItems[0]) == MENU_COUNT,
              "oledRenderIcon4MenuItems[] must stay in sync with the MenuItem enum");

const uint8_t OLED_HEAD = 13;
const uint8_t OLED_LINE = 17;
const uint8_t OLED_L3_1 = 30;
const uint8_t OLED_L3_2 = 43;
const uint8_t OLED_L3_3 = 56;

// Bottom selectable row (dialog buttons, SSID / keyboard cells): frame top and
// text baseline. These coincide with OLED_L3_2/L3_3 but read as a row, not as
// text lines. OLED_DIALOG_HEAD_H is the dialog header box, sitting above the row.
const uint8_t OLED_ROW_TOP = 43;
const uint8_t OLED_ROW_BASE = 56;
const uint8_t OLED_DIALOG_HEAD_H = 41;

const uint8_t PLACE_CARD_W = 64;                            // one place card / menu-icon cell
const uint16_t PLACES_ERROR_W = OLED_WIDTH - PLACE_CARD_W;  // error text area beside the menu icon
const uint8_t WIFI_SSID_CELL_W = 128;                       // one SSID list cell

//const char* wd_short[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

// --------------------------------------------------------------- MAIN

void oledSetup() {
  SPI.begin(PIN_OLED_CLK, -1, PIN_OLED_MOSI, PIN_OLED_CS);
  u8g2.begin();
  u8g2.setBusClock(8000000);

  u8g2.setContrast(50);
  oledFlipGet();
}

void oledBufferClear() {
  u8g2.clearBuffer();
}

void oledBufferSend() {
  u8g2.sendBuffer();
}

void oledEnabled(bool enabled) {
  if (enabled) {
    u8g2.setPowerSave(0);
  } else {
    u8g2.setPowerSave(1);
  }
}

// --------------------------------------------------------------- PREFERENCES

uint8_t oledFlipGet() {
  oledPref.begin(OLED_PREF, true);
  uint8_t flip = oledPref.getBool(OLED_PREF_FLIP, false);
  oledPref.end();

  u8g2.setFlipMode(flip);
  return flip;
}

void oledFlipSet(bool flip) {
  oledPref.begin(OLED_PREF, false);
  oledPref.putBool(OLED_PREF_FLIP, flip);
  oledPref.end();

  u8g2.setFlipMode(flip);
}

// --------------------------------------------------------------- COMMON

void oledSetFontStandard() {
  u8g2.setFont(u8g2_font_profont12_mf);
}

void oledGlyph(uint8_t x, uint8_t y, const OledRenderIcon& ori) {
  u8g2.setFont(ori.font);
  u8g2.drawGlyph(x, y, ori.icon);
}

// --------------------------------------------------------------- PARTIALS

void oledRenderSelected(uint8_t x, uint8_t w) {
  u8g2.drawLine(x + 1, OLED_HEIGHT - 1, x + w - 2, OLED_HEIGHT - 1);
  u8g2.drawLine(x + 1, OLED_HEIGHT - 2, x + w - 2, OLED_HEIGHT - 2);
}

uint8_t oledRenderBox(uint8_t x, uint16_t w, uint8_t h, const char* name, bool center, bool selected) {
  u8g2.drawFrame(x + 1, 0, w - 2, h);

  oledSetFontStandard();
  int8_t off;
  if (center) {
    off = (w / 2) - (u8g2.getStrWidth(name) / 2);
  } else {
    off = 6;
  }
  u8g2.drawStr(x + off, OLED_HEAD, name);

  u8g2.drawLine(x + 2 + 4, OLED_LINE, x + w - 4 - 2 - 1, OLED_LINE);

  if (selected) {
    oledRenderSelected(x, w);
  }

  return w;
}

uint8_t oledRenderBoxIcon(uint8_t x, uint8_t w, const OledRenderIcon& ori, bool selected) {
  oledRenderBox(x, w, OLED_HEIGHT - 3, ori.name, true, selected);

  u8g2.setFont(ori.font);
  uint8_t offh = (w / 2) - 16;
  uint8_t offv = OLED_LINE + 16 + ((62 - OLED_LINE) / 2);
  u8g2.drawGlyph(x + offh, offv, ori.icon);

  return w;
}

uint8_t oledRenderPlace(uint8_t x, uint8_t pidx, bool selected) {
  char name[PLACES_NAME_MAX];
  placesName(pidx, name, sizeof name);
  const uint8_t w = oledRenderBox(x, PLACE_CARD_W, OLED_HEIGHT - 3, name, true, selected);

  uint16_t untilChange;
  bool open = placesStatus(pidx, &untilChange);

  // Single countdown HH:MM to the next status change, redrawn every minute (the
  // loop's minute-change detection). Hours are capped at 99 for the 64px card.
  uint16_t hh = untilChange / 60;
  uint16_t mm = untilChange % 60;
  if (hh > 99) {
    hh = 99;
    mm = 59;
  }
  // "12h45m" form so it reads as a duration, not a clock time; drop the hours
  // below one hour ("45m").
  char strLine[14];
  if (hh > 0) {
    snprintf(strLine, sizeof strLine, "%dh %02dm", hh, mm);
  } else {
    snprintf(strLine, sizeof strLine, "%dm", mm);
  }

  // Draw all text first: oledGlyph() switches to the icon font, so the glyphs
  // must come after the text or the digits render in the wrong font (blank).
  // Right-align the (variable-width) countdown to the status label's right edge.
  oledSetFontStandard();
  u8g2.drawStr(x + 29, OLED_L3_1, open ? " OPEN" : "CLOSE");
  int rightEdge = 29 + u8g2.getStrWidth("CLOSE");
  u8g2.drawStr(x + rightEdge - u8g2.getStrWidth(strLine), OLED_L3_3, strLine);

  // Status icon (check/X) and countdown-direction icon (locked = will close,
  // unlocked = will open).
  oledGlyph(x + 6, OLED_L3_1, open ? oledRenderIcon1Check : oledRenderIcon1X);
  oledGlyph(x + 5, OLED_L3_3, open ? oledRenderIcon1Locked : oledRenderIcon1Unlocked);

  return w;
}

uint8_t oledRenderPlaceDay(uint8_t x, uint8_t pidx, uint8_t didx, bool selected) {
  const uint8_t w = 96;

  // didx is the display column, Monday-first (0=Mon .. 6=Sun); map it to the
  // internal Sunday-based day (0=Sun) used by the week-minute encoding.
  const char* DAYS = "Mon\0Tue\0Wed\0Thu\0Fri\0Sat\0Sun\0";
  oledRenderBox(x, w, OLED_HEIGHT - 3, DAYS + (4 * didx), false, selected);

  uint8_t day = (didx + 1) % DAYS_PER_WEEK;
  uint16_t dayStart = day * MINUTES_PER_DAY;
  uint16_t dayEnd = dayStart + MINUTES_PER_DAY;

  oledSetFontStandard();
  uint8_t off = (w / 2) - (u8g2.getStrWidth("             ") / 2);

  uint8_t line = 0;
  for (uint8_t s = 0; s < placesSlotCount(pidx) && line < 3; s++) {
    uint32_t slot = placesSlot(pidx, s);
    uint16_t from = placesSlotFrom(slot);
    uint16_t to = placesSlotTo(slot);

    // Normalise into up to two non-wrapping [a,b) intervals, then clamp to this
    // day's window, so a slot crossing midnight shows on both days (e.g.
    // "22:00 - 24:00" on one, "00:00 - 02:00" on the next).
    uint16_t seg[2][2];
    uint8_t segs = 0;
    if (to > from) {
      seg[segs][0] = from;
      seg[segs][1] = to;
      segs++;
    } else if (to < from) {
      seg[segs][0] = from;
      seg[segs][1] = MINUTES_PER_WEEK;
      segs++;
      seg[segs][0] = 0;
      seg[segs][1] = to;
      segs++;
    }

    for (uint8_t k = 0; k < segs && line < 3; k++) {
      uint16_t a = seg[k][0] > dayStart ? seg[k][0] : dayStart;
      uint16_t b = seg[k][1] < dayEnd ? seg[k][1] : dayEnd;
      if (a >= b) {
        continue;  // segment does not touch this day
      }
      uint16_t os = a - dayStart;  // 0 .. 1439
      uint16_t oe = b - dayStart;  // 1 .. 1440 (1440 renders as 24:00)

      char strLine[14];
      snprintf(strLine, sizeof strLine, "%02d:%02d - %02d:%02d", os / 60, os % 60, oe / 60, oe % 60);
      if (line == 0) {
        u8g2.drawStr(x + off, OLED_L3_1, strLine);
      }
      if (line == 1) {
        u8g2.drawStr(x + off, OLED_L3_2, strLine);
      }
      if (line == 2) {
        u8g2.drawStr(x + off, OLED_L3_3, strLine);
      }
      line++;
    }
  }

  if (selected) {
    oledRenderSelected(x, w);
  }

  return w;
}

uint8_t oledRenderWifiSsid(uint8_t x, const char* ssid, bool selected) {
  u8g2.drawFrame(x + 1, OLED_ROW_TOP, WIFI_SSID_CELL_W - 2, OLED_HEIGHT - OLED_ROW_TOP - 3);

  oledSetFontStandard();
  int8_t off = (WIFI_SSID_CELL_W / 2) - (u8g2.getStrWidth(ssid) / 2);
  u8g2.drawStr(x + off, OLED_ROW_BASE, ssid);

  if (selected) {
    oledRenderSelected(x, WIFI_SSID_CELL_W);
  }
  return WIFI_SSID_CELL_W;
}

void oledRenderWifiPassHeader(const char* ssid, const char* pass) {
  u8g2.drawFrame(0 + 2, 0, OLED_WIDTH - 2, OLED_ROW_TOP - 2 - 2);
  oledSetFontStandard();

  char line[48];
  snprintf(line, sizeof line, "SSID: %.33s", ssid);
  u8g2.drawStr(7, 13, line);

  snprintf(line, sizeof line, "PASS: %s", pass);
  u8g2.drawStr(7, 24, line);
}

uint8_t oledRenderWifiPassLetter(uint8_t x, uint8_t idx, bool selected) {
  const uint8_t w = 16;

  u8g2.drawFrame(x + 2, OLED_ROW_TOP, w - 2, OLED_HEIGHT - 3 - OLED_ROW_TOP);

  char c;
  switch (wifiPassSlotKey(idx, &c)) {
    case WIFI_PASS_KEY_BACK:
      oledGlyph(x + 5, OLED_ROW_BASE, oledRenderIcon1Back);
      break;
    case WIFI_PASS_KEY_OK:
      oledGlyph(x + 5, OLED_ROW_BASE, oledRenderIcon1Check);
      break;
    case WIFI_PASS_KEY_DEL:
      oledGlyph(x + 5, OLED_ROW_BASE, oledRenderIcon1X);
      break;
    case WIFI_PASS_KEY_CHAR:
      oledSetFontStandard();
      u8g2.drawGlyph(x + 6, OLED_ROW_BASE, c);
      break;
  }

  if (selected) {
    oledRenderSelected(x, w);
  }

  return w;
}

// --------------------------------------------------------------- SCREENS

void oledShowSplash() {
  u8g2.clearBuffer();

  u8g2.drawFrame(0, 0, OLED_WIDTH, OLED_HEIGHT);
  u8g2.drawFrame(2, 2, OLED_WIDTH - 4, OLED_HEIGHT - 4);

  uint8_t off = 0;

  u8g2.setFont(u8g2_font_logisoso22_tr);
  off = (OLED_WIDTH / 2) - (u8g2.getStrWidth(appName) / 2);
  u8g2.drawStr(off, 34, appName);

  u8g2.drawHLine(20, 40, OLED_WIDTH - 40);

  oledSetFontStandard();
  off = (OLED_WIDTH / 2) - (u8g2.getStrWidth(appVersion) / 2);
  u8g2.drawStr(off, 55, appVersion);

  u8g2.sendBuffer();
}

void oledShowDialog(const char* head, const char* msg, uint8_t btnCount, const char* btns[], uint8_t selected) {
  oledRenderBox(0, OLED_WIDTH, OLED_DIALOG_HEAD_H, head, true, false);

  oledSetFontStandard();
  uint8_t off = (OLED_WIDTH / 2) - (u8g2.getStrWidth(msg) / 2);
  u8g2.drawStr(off, 33, msg);

  if (btnCount == 0) {
    return;
  }

  uint16_t x = 0;
  uint16_t w = OLED_WIDTH / btnCount;

  for (uint8_t i = 0; i < btnCount; i++) {
    u8g2.drawFrame(x + 1, OLED_ROW_TOP, w - 2, OLED_HEIGHT - OLED_ROW_TOP - 3);
    off = (w / 2) - (u8g2.getStrWidth(btns[i]) / 2);
    u8g2.drawStr(x + off, OLED_ROW_BASE, btns[i]);
    if (selected == i) {
      oledRenderSelected(x, w);
    }
    x += w;
  }
}

void oledRenderConnectionStatusIcon(uint8_t x, uint8_t y, uint8_t state) {
  if (state == 0) {
    oledGlyph(x, y, oledRenderIcon1Pause);
  }
  if (state == 1) {
    oledGlyph(x, y, oledRenderIcon1Start);
  }
  if (state == 2) {
    oledGlyph(x, y, oledRenderIcon1Check);
  }
  if (state == 3) {
    oledGlyph(x, y, oledRenderIcon1X);
  }
}

// Live progress screen for a blocking network op (wifi/data each 0..3, see
// oledRenderConnectionStatusIcon). Unlike the oledShow* screens driven by
// loopOled() — which brackets them with clear/send — this is called inline from
// mainNetwork() while loop() is parked, so it manages the buffer itself. It and
// the setup-time oledShowSplash are the only sanctioned draws outside the render
// phase; keep it that way.
void oledShowConnectionStatus(uint8_t wifi, uint8_t data) {
  u8g2.clearBuffer();

  u8g2.drawFrame(0, 0, OLED_WIDTH, OLED_HEIGHT);

  oledGlyph(16, 48, oledRenderIcon4MenuItems[2]);
  u8g2.drawLine(OLED_HEIGHT, 0, OLED_HEIGHT, OLED_HEIGHT);

  oledSetFontStandard();
  u8g2.drawStr(OLED_HEIGHT + 18, 24, "WiFi connection");
  oledRenderConnectionStatusIcon(OLED_WIDTH - 5 - 18, 25, wifi);

  oledSetFontStandard();
  u8g2.drawStr(OLED_HEIGHT + 18, 49, "Data transfer");
  oledRenderConnectionStatusIcon(OLED_WIDTH - 5 - 18, 50, data);

  u8g2.sendBuffer();
}

void oledShowPlacesList(uint8_t navStart, uint8_t entries, uint8_t selected) {
  uint8_t x = 0;
  for (uint8_t i = 0; i < entries; i++) {
    if (navStart + i < placesCount()) {
      x += oledRenderPlace(x, navStart + i, selected == i);
    } else {
      x += oledRenderBoxIcon(x, PLACE_CARD_W, oledRenderIcon4Menu, selected == i);
    }
  }
}

void oledShowPlacesListError(const char* text1, const char* text2, uint8_t selected) {
  oledRenderBoxIcon(0, PLACE_CARD_W, oledRenderIcon4Menu, selected == 0);
  oledRenderBox(PLACE_CARD_W, PLACES_ERROR_W, OLED_HEIGHT - 3, "Network Error", true, selected == 1);

  uint8_t off;
  if (text1 && text1[0] != '\0') {
    oledSetFontStandard();
    off = (PLACES_ERROR_W / 2) - (u8g2.getStrWidth(text1) / 2);
    u8g2.drawStr(PLACE_CARD_W + off, 40 - 5, text1);
  }
  if (text2 && text2[0] != '\0') {
    oledSetFontStandard();
    off = (PLACES_ERROR_W / 2) - (u8g2.getStrWidth(text2) / 2);
    u8g2.drawStr(PLACE_CARD_W + off, 40 + 12, text2);
  }
}

void oledShowPlacesDetail(uint8_t pidx, uint8_t navStart, uint8_t selected) {
  if (pidx >= placesCount()) {
    return;
  }
  uint8_t x = 0;
  x += oledRenderPlace(0, pidx, false);
  for (uint8_t i = 0; i < DETAIL_DAYS_PER_PAGE; i++) {
    x += oledRenderPlaceDay(x, pidx, navStart + i, selected == i);
  }
}

void oledShowWifiSsid(uint8_t navStart, uint8_t selected) {
  oledShowDialog("WiFi Scanning", "Select SSID", 0, NULL, 0);
  uint8_t x = 0;
  for (uint8_t i = 0; i < WIFI_SSID_PER_PAGE; i++) {
    uint8_t idx = navStart + i;
    if (idx == 0) {
      x += oledRenderWifiSsid(x, "Back", selected == i);
    } else if (idx == wifiScanLen() + 1) {
      x += oledRenderWifiSsid(x, "Rescan", selected == i);
    } else if (idx == wifiScanLen() + 2) {
      x += oledRenderWifiSsid(x, "Manual", selected == i);
    } else {
      char ssid[19];
      snprintf(ssid, sizeof ssid, "%.18s", wifiScanItems()[idx - 1].c_str());
      x += oledRenderWifiSsid(x, ssid, selected == i);
    }
  }
}

void oledShowWifiPass(const char* ssid, const char* pass, uint8_t navStart, uint8_t selected) {
  uint8_t x = 0;
  oledRenderWifiPassHeader(ssid, pass);
  for (uint8_t i = 0; i < WIFI_PASS_PER_PAGE; i++) {
    x += oledRenderWifiPassLetter(x, navStart + i, selected == i);
  }
}

void oledShowWifiSsidManual(const char* ssid, uint8_t navStart, uint8_t selected) {
  u8g2.drawFrame(0 + 2, 0, OLED_WIDTH - 2, OLED_ROW_TOP - 2 - 2);
  oledSetFontStandard();

  char line[48];
  snprintf(line, sizeof line, "SSID: %s", ssid);
  u8g2.drawStr(7, 13, line);
  u8g2.drawStr(7, 24, "Enter network name, then OK");

  uint8_t x = 0;
  for (uint8_t i = 0; i < WIFI_PASS_PER_PAGE; i++) {
    x += oledRenderWifiPassLetter(x, navStart + i, selected == i);
  }
}

void oledShowMenu(uint8_t navStart, uint8_t selected) {
  uint8_t x = 0;
  for (uint8_t i = 0; i < MENU_PER_PAGE; i++) {
    x += oledRenderBoxIcon(x, 64, oledRenderIcon4MenuItems[navStart + i], selected == i);
  }
}
