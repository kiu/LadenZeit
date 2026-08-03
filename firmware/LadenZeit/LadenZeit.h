#ifndef LZ_MAIN_H
#define LZ_MAIN_H

#include <Arduino.h>

#define SLEEP_IN_SECONDS 120

extern const char* appName;
extern const char* appVersion;

enum AppState {
  STATE_INIT,

  STATE_PLACES_LIST,
  STATE_PLACES_LIST_ERROR,
  STATE_PLACES_DETAIL,

  STATE_MENU,
  STATE_MENU_PLACES,
  STATE_MENU_PLACES_FAIL,
  STATE_MENU_PLACES_CODE,
  STATE_MENU_WIFI_SSID,
  STATE_MENU_WIFI_SSID_RES,
  STATE_MENU_WIFI_SSID_MANUAL,
  STATE_MENU_WIFI_PASS,
  STATE_MENU_WIFI_TEST,
  STATE_MENU_WIFI_TEST_RUN,
  STATE_MENU_WIFI_TEST_OK,
  STATE_MENU_WIFI_TEST_FAIL,
  STATE_MENU_HANDED,
  STATE_MENU_FACTORY,

  STATE_COUNT,  // sentinel: number of states; keep last
};

// Main menu entries, in display order. Must match oledRenderIcon4MenuItems[] in
// lz_oled.cpp (guarded by a static_assert there). MENU_COUNT is the item count.
enum MenuItem {
  MENU_BACK,
  MENU_PLACES,
  MENU_WIFI,
  MENU_REFRESH,
  MENU_HANDED,
  MENU_FACTORY,

  MENU_COUNT,  // sentinel: keep last
};

void mainOledUpdate();

#endif