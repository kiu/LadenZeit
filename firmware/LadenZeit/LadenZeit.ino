#include "LadenZeit.h"

#include "lz_config.h"

#include <Preferences.h>
#include <nvs_flash.h>
#include <esp_sleep.h>

#include "lz_dial.h"
#include "lz_network.h"
#include "lz_oled.h"
#include "lz_places.h"
#include "lz_time.h"

const char* appName = "LadenZeit";
const char* appVersion = "v0.0.1";
const char* appRev = "REV B";

volatile bool oledUpdate = true;

AppState state = STATE_INIT;
AppState stateNext = STATE_INIT;
AppState statePrev = STATE_INIT;
AppState stateSsidPrev = STATE_INIT;

uint8_t mainNavCount = 0;
uint8_t mainNavPerPage = 0;
uint8_t mainNavStart = 0;
uint8_t mainNavSelected = 0;

uint8_t mainNavStartLast = 0;
uint8_t mainNavSelectedLast = 0;

// Place whose detail screen is shown, captured when it's picked from the list
// (the detail state reuses the nav window for its day columns).
uint8_t mainPlaceSelected = 0;

String mainPass = "";
String mainSsid = "";

const char* btnsHanded[] = { "Lefty", "Righty" };
const char* btnsFactory[] = { "Back", "Reset" };
const char* btnsMenuPlaces[] = { "Back", "Retry" };
const char* btnsOk[] = { "OK" };
const char* btnsWifiFail[] = { "Back", "Retry" };

NetworkState mainNetworkStatePlaces = NETWORK_OK;
bool mainNetworkPlacesForceUpdate = false;

bool startFresh = true;

time_t mainActionLast;

// One row per screen in the STATES[] dispatch table (defined lower down, after
// the handler functions it points at). See the STATE MACHINE section for how the
// loop* passes use these three handlers.
struct StateDef {
  void (*onEnter)();
  void (*onButton)();
  void (*onRender)();
};

// --------------------------------------------------------------- MAIN

void mainOledUpdate() {
  oledUpdate = true;
}

void mainFactoryReset() {
  oledBufferClear();
  oledBufferSend();

  nvs_flash_erase();  // Erases the entire NVS partition
  nvs_flash_init();   // Re-initializes it for fresh use

  ESP.restart();
}

// --------------------------------------------------------------- NETWORK

// Runs a blocking connect + request (called from onEnter handlers), so loop()
// is parked for the duration. Progress is pushed straight to the panel via
// oledShowConnectionStatus() rather than the loopOled() render phase — the one
// intentional exception to "rendering goes through onRender". Deliberately kept
// synchronous: the device has nothing else to do while a fetch is in flight.
NetworkState mainNetwork(NetworkState (*func)()) {
  oledShowConnectionStatus(1, 0);

  NetworkState result;

  result = wifiConnect();
  if (result == NETWORK_OK) {
    oledShowConnectionStatus(2, 0);
  } else {
    oledShowConnectionStatus(3, 0);
  }

  if (result == NETWORK_OK) {
    oledShowConnectionStatus(2, 1);
    result = func();
    if (result == NETWORK_OK) {
      oledShowConnectionStatus(2, 2);
    } else {
      oledShowConnectionStatus(2, 3);
    }
    wifiDisconnect();
  }

  return result;
}

NetworkState mainNetworkPlacesDownload() {
  bool run = false;

  run |= mainNetworkPlacesForceUpdate;
  run |= timeCacheExpired();
  run |= !placesValid();  // no valid data (e.g. last refresh rejected) — must re-fetch

  mainNetworkPlacesForceUpdate = false;

  if (!run) {
    return NETWORK_OK;
  }

  NetworkState result = mainNetwork(httpDownloadPlaces);
  if (result == NETWORK_OK) {
    placesUpdateTime();
    placesDebug();
    timeCacheInit();
  }
  return result;
}

NetworkState mainNetworkPlacesConfiguration() {
  return mainNetwork(httpEnableConfig);
}

// --------------------------------------------------------------- NAVIGATION

// Absolute index of the current selection within the nav window.
uint8_t mainNavIndex() {
  return mainNavStart + mainNavSelected;
}

// Single-page nav window (perPage == count, no scrolling): the common shape for
// confirm dialogs. Sets the selected item; start is reset to 0.
void mainNavSet(uint8_t count, uint8_t selected) {
  mainNavStart = 0;
  mainNavCount = count;
  mainNavPerPage = count;
  mainNavSelected = selected;
}

bool mainNavDial(int8_t diff) {
  //LOG("A Diff: %d C:%d S:%d P:%d S:%d\n", diff, mainNavCount, mainNavStart, mainNavPerPage, mainNavSelected);

  if (mainNavCount == 0) {
    return false;
  }
  if (mainNavCount == 1) {
    return false;
  }

  while (diff > 0) {
    diff--;
    if (mainNavSelected < mainNavPerPage - 1) {
      if (mainNavSelected < mainNavCount - 1) {
        mainNavSelected++;
      }
    } else {
      if (mainNavStart + mainNavPerPage < mainNavCount) {
        mainNavStart++;
      }
    }
  }

  while (diff < 0) {
    diff++;
    if (mainNavSelected > 0) {
      mainNavSelected--;
    } else {
      if (mainNavStart > 0) {
        mainNavStart--;
      }
    }
  }

  //LOG("B Diff: %d C:%d S:%d P:%d S:%d\n", diff, mainNavCount, mainNavStart, mainNavPerPage, mainNavSelected);

  return true;
}

// --------------------------------------------------------------- STATE MACHINE
//
// Each screen is one row in the STATES[] table below, holding up to three
// handlers dispatched by the generic loop* functions:
//   onEnter()  - runs once on transition into the state; sets nav counts /
//                side effects, and may set stateNext (for action-only states).
//   onButton() - runs on a dial button press.
//   onRender() - redraws the screen (called when oledUpdate is set).
// A null handler means "nothing to do"; loop* logs an undefined-state notice
// when a state is reached with no handler for that phase (matching the old
// switch defaults). STATE_MENU_PLACES keeps an empty (non-null) render so it
// stays silent, as before. StateDef itself is declared up top with the globals.

// ---- onEnter handlers ----

void stPlacesListEnter() {
  stateSsidPrev = STATE_PLACES_LIST;
  mainNetworkStatePlaces = mainNetworkPlacesDownload();
  mainNavCount = placesCount() + 1;
  mainNavPerPage = PLACES_PER_PAGE;
  if (mainNetworkStatePlaces != NETWORK_OK) {
    stateNext = STATE_PLACES_LIST_ERROR;
  }
}

void stPlacesErrorEnter() {
  mainNavSet(2, 1);
}

void stPlacesDetailEnter() {
  // Open on today's column. timeDayGet() is 0=Sunday; the detail columns are
  // Monday-first, so today's position is (day + 6) % 7.
  uint8_t pos = (timeDayGet() + DAYS_PER_WEEK - 1) % DAYS_PER_WEEK;
  mainNavCount = DAYS_PER_WEEK;
  mainNavPerPage = DETAIL_DAYS_PER_PAGE;
  if (pos + DETAIL_DAYS_PER_PAGE <= DAYS_PER_WEEK) {
    mainNavStart = pos;
    mainNavSelected = 0;
  } else {
    mainNavStart = DAYS_PER_WEEK - DETAIL_DAYS_PER_PAGE;
    mainNavSelected = pos - mainNavStart;
  }
}

void stMenuEnter() {
  mainNavCount = MENU_COUNT;
  mainNavPerPage = MENU_PER_PAGE;
  stateSsidPrev = STATE_MENU;
}

void stMenuPlacesEnter() {
  mainNavCount = 0;
  mainNavPerPage = 0;
  NetworkState result = mainNetworkPlacesConfiguration();
  if (result == NETWORK_OK) {
    stateNext = STATE_MENU_PLACES_CODE;
  } else {
    stateNext = STATE_MENU_PLACES_FAIL;
  }
}

void stMenuPlacesCodeEnter() {
  mainNavSet(1, 0);
}

void stMenuPlacesFailEnter() {
  mainNavSet(2, 1);
}

void stMenuWifiSsidEnter() {
  mainNavStart = 0;
  mainNavCount = 0;
  mainNavPerPage = 0;
  mainNavSelected = 0;
  stateNext = STATE_MENU_WIFI_SSID_RES;
}

void stMenuWifiSsidResEnter() {
  mainNavStart = 0;
  mainNavSelected = 0;
  // Scan only when reached via the scan trigger (main menu, error recovery, or
  // Rescan). Returning from the pass/manual screens reuses the cached results,
  // so navigating back no longer forces a fresh blocking scan every time.
  if (statePrev == STATE_MENU_WIFI_SSID) {
    wifiScan();
  }
  // scanned SSIDs + 3 control rows: Back (top), Rescan and Manual (bottom).
  mainNavCount = wifiScanLen() + 3;
  mainNavPerPage = WIFI_SSID_PER_PAGE;
}

void stMenuWifiSsidManualEnter() {
  mainNavCount = wifiPassSlotCount();
  mainNavPerPage = WIFI_PASS_PER_PAGE;
  if (startFresh) {
    startFresh = false;
    mainSsid = "";
  }
}

void stMenuWifiPassEnter() {
  mainNavCount = wifiPassSlotCount();
  mainNavPerPage = WIFI_PASS_PER_PAGE;
  if (startFresh) {
    startFresh = false;
    mainPass = "";
  }
}

void stMenuWifiTestEnter() {
  stateNext = STATE_MENU_WIFI_TEST_RUN;
}

void stMenuWifiTestRunEnter() {
  if (wifiConnectAndStore(mainSsid, mainPass) == NETWORK_OK) {
    stateNext = STATE_MENU_WIFI_TEST_OK;
  } else {
    stateNext = STATE_MENU_WIFI_TEST_FAIL;
  }
}

void stMenuWifiTestOkEnter() {
  mainNavSet(1, 0);
}

void stMenuWifiTestFailEnter() {
  mainNavSet(2, 0);
}

void stMenuHandedEnter() {
  mainNavSet(2, !oledFlipGet());
}

void stMenuFactoryEnter() {
  mainNavSet(2, 0);
}

// ---- onButton handlers ----

void stPlacesListButton() {
  if (mainNavIndex() == mainNavCount - 1) {
    stateNext = STATE_MENU;
  } else {
    mainPlaceSelected = mainNavIndex();
    stateNext = STATE_PLACES_DETAIL;
  }
}

void stPlacesErrorButton() {
  switch (mainNavIndex()) {
    case 0:
      stateNext = STATE_MENU;
      break;

    case 1:
      if (mainNetworkStatePlaces == NETWORK_ERROR_CONFIGURATION) {
        stateNext = STATE_MENU_WIFI_SSID;
      } else if (mainNetworkStatePlaces == NETWORK_ERROR_CONNECTION) {
        stateNext = STATE_PLACES_LIST;
      } else if (mainNetworkStatePlaces == NETWORK_ERROR_DATA) {
        stateNext = STATE_PLACES_LIST;
      } else {
        LOG("main: undefined error state for button press in network error (%d)\n", mainNavIndex());
      }
      break;

    default:
      LOG("main: undefined menu selected for button press in network error (%d)\n", mainNavIndex());
      break;
  }
}

void stPlacesDetailButton() {
  stateNext = STATE_PLACES_LIST;
}

void stMenuButton() {
  switch (mainNavIndex()) {
    case MENU_BACK:
      stateNext = STATE_PLACES_LIST;
      break;

    case MENU_PLACES:
      stateNext = STATE_MENU_PLACES;
      break;

    case MENU_WIFI:
      stateNext = STATE_MENU_WIFI_SSID;
      break;

    case MENU_REFRESH:
      mainNetworkPlacesForceUpdate = true;
      stateNext = STATE_PLACES_LIST;
      break;

    case MENU_HANDED:
      stateNext = STATE_MENU_HANDED;
      break;

    case MENU_FACTORY:
      stateNext = STATE_MENU_FACTORY;
      break;

    default:
      LOG("main: undefined menu selected for button press (%d)\n", mainNavIndex());
      break;
  }
}

void stMenuPlacesCodeButton() {
  stateNext = STATE_PLACES_LIST;
  mainNetworkPlacesForceUpdate = true;
}

void stMenuPlacesFailButton() {
  if (mainNavSelected == 0) {
    stateNext = STATE_MENU;
  } else {
    stateNext = STATE_MENU_PLACES;
  }
}

void stMenuWifiSsidResButton() {
  uint8_t idx = mainNavIndex();
  if (idx == 0) {
    stateNext = stateSsidPrev;
  } else if (idx == wifiScanLen() + 1) {
    stateNext = STATE_MENU_WIFI_SSID;
  } else if (idx == wifiScanLen() + 2) {
    startFresh = true;
    stateNext = STATE_MENU_WIFI_SSID_MANUAL;
  } else {
    startFresh = true;
    stateNext = STATE_MENU_WIFI_PASS;
    mainSsid = wifiScanItems()[idx - 1];
    LOG("wifi: Selected SSID (%02d) %s\n", idx, mainSsid.c_str());
  }
}

void stMenuWifiSsidManualButton() {
  char c;
  switch (wifiPassSlotKey(mainNavIndex(), &c)) {
    case WIFI_PASS_KEY_BACK:
      stateNext = STATE_MENU_WIFI_SSID_RES;  // cached list, no re-scan
      break;

    case WIFI_PASS_KEY_OK:
      startFresh = true;  // clear the password field on entry
      stateNext = STATE_MENU_WIFI_PASS;
      break;

    case WIFI_PASS_KEY_DEL:
      oledUpdate = true;
      if (mainSsid.length() > 0) {
        mainSsid = mainSsid.substring(0, mainSsid.length() - 1);
      }
      break;

    case WIFI_PASS_KEY_CHAR:
      if (mainSsid.length() < WIFI_SSID_LEN) {
        oledUpdate = true;
        mainSsid += c;
      }
      break;
  }
}

void stMenuWifiPassButton() {
  char c;
  switch (wifiPassSlotKey(mainNavIndex(), &c)) {
    case WIFI_PASS_KEY_BACK:
      stateNext = STATE_MENU_WIFI_SSID_RES;  // cached list, no re-scan
      break;

    case WIFI_PASS_KEY_OK:
      stateNext = STATE_MENU_WIFI_TEST;
      break;

    case WIFI_PASS_KEY_DEL:
      oledUpdate = true;
      if (mainPass.length() > 0) {
        mainPass = mainPass.substring(0, mainPass.length() - 1);
      }
      break;

    case WIFI_PASS_KEY_CHAR:
      if (mainPass.length() < WIFI_PASS_MAX_LEN) {
        oledUpdate = true;
        mainPass += c;
      }
      break;
  }
}

void stMenuWifiTestOkButton() {
  switch (mainNavIndex()) {
    case 0:
      stateNext = stateSsidPrev;
      break;

    default:
      LOG("main: undefined menu selected for button press in wifi test ok (%d)\n", mainNavIndex());
      break;
  }
}

void stMenuWifiTestFailButton() {
  switch (mainNavIndex()) {
    case 0:
      stateNext = STATE_MENU_WIFI_PASS;
      break;

    case 1:
      stateNext = STATE_MENU_WIFI_TEST;
      break;

    default:
      LOG("main: undefined menu selected for button press in wifi test fail (%d)\n", mainNavIndex());
      break;
  }
}

void stMenuHandedButton() {
  stateNext = STATE_MENU;
  switch (mainNavIndex()) {
    case 0:
      oledFlipSet(true);
      break;

    case 1:
      oledFlipSet(false);
      break;

    default:
      LOG("main: undefined menu selected for button press in handed (%d)\n", mainNavIndex());
      break;
  }
}

void stMenuFactoryButton() {
  switch (mainNavIndex()) {
    case 0:
      stateNext = STATE_MENU;
      break;

    case 1:
      mainFactoryReset();
      break;

    default:
      LOG("main: undefined menu selected for button press in factory (%d)\n", mainNavIndex());
      break;
  }
}

// ---- onRender handlers ----

void stPlacesListRender() {
  oledShowPlacesList(mainNavStart, (mainNavCount > mainNavStart + PLACES_PER_PAGE) ? mainNavPerPage : mainNavCount - mainNavStart, mainNavSelected);
}

void stPlacesErrorRender() {
  if (mainNetworkStatePlaces == NETWORK_ERROR_CONFIGURATION) {
    oledShowPlacesListError("WiFi connection not setup", "Select to configure", mainNavIndex());
  }
  if (mainNetworkStatePlaces == NETWORK_ERROR_CONNECTION) {
    oledShowPlacesListError("WiFi connection failed", "Select to retry", mainNavIndex());
  }
  if (mainNetworkStatePlaces == NETWORK_ERROR_DATA) {
    oledShowPlacesListError("Data transmission failed", "Select to retry", mainNavIndex());
  }
}

void stPlacesDetailRender() {
  oledShowPlacesDetail(mainPlaceSelected, mainNavStart, mainNavSelected);
}

void stMenuRender() {
  oledShowMenu(mainNavStart, mainNavSelected);
}

void stMenuPlacesRender() {
  // intentionally blank: transient config state draws nothing (was an empty
  // switch case; kept non-null so it stays silent instead of logging undefined)
}

void stMenuPlacesCodeRender() {
  char code[24];
  snprintf(code, sizeof code, "Code: %s", httpPasscodeHuman.c_str());
  oledShowDialog("ladenzeit.nakamura-labs.com", code, 1, btnsOk, mainNavSelected);
}

void stMenuPlacesFailRender() {
  oledShowDialog("Network Error", "Failed to enable web interface", 2, btnsMenuPlaces, mainNavSelected);
}

void stMenuWifiSsidRender() {
  oledShowDialog("WiFi Scanning", "Please wait ...", 0, NULL, 0);
}

void stMenuWifiSsidResRender() {
  oledShowWifiSsid(mainNavStart, mainNavSelected);
}

void stMenuWifiSsidManualRender() {
  oledShowWifiSsidManual(mainSsid.c_str(), mainNavStart, mainNavSelected);
}

void stMenuWifiPassRender() {
  oledShowWifiPass(mainSsid.c_str(), mainPass.c_str(), mainNavStart, mainNavSelected);
}

void stMenuWifiTestRender() {
  oledShowDialog("WiFi Connection", "Trying connection", 0, NULL, 0);
}

void stMenuWifiTestOkRender() {
  oledShowDialog("WiFi Connection", "Connection OK", 1, btnsOk, 0);
}

void stMenuWifiTestFailRender() {
  oledShowDialog("WiFi Connection", "Connection failed", 2, btnsWifiFail, mainNavSelected);
}

void stMenuHandedRender() {
  oledShowDialog("Handedness", "Set your main hand", 2, btnsHanded, mainNavSelected);
}

void stMenuFactoryRender() {
  oledShowDialog("Factory Reset", "Are you sure?", 2, btnsFactory, mainNavSelected);
}

// ---- dispatch table ----
// Positional, one row per AppState. Order MUST match the enum in LadenZeit.h;
// the static_assert below guards the row count (not the ordering, so keep the
// name comments in sync). A field left nullptr means "no handler for that phase".

static const StateDef STATES[STATE_COUNT] = {
  /* STATE_INIT                */ { nullptr, nullptr, nullptr },
  /* STATE_PLACES_LIST         */ { stPlacesListEnter, stPlacesListButton, stPlacesListRender },
  /* STATE_PLACES_LIST_ERROR   */ { stPlacesErrorEnter, stPlacesErrorButton, stPlacesErrorRender },
  /* STATE_PLACES_DETAIL       */ { stPlacesDetailEnter, stPlacesDetailButton, stPlacesDetailRender },
  /* STATE_MENU                */ { stMenuEnter, stMenuButton, stMenuRender },
  /* STATE_MENU_PLACES         */ { stMenuPlacesEnter, nullptr, stMenuPlacesRender },
  /* STATE_MENU_PLACES_FAIL    */ { stMenuPlacesFailEnter, stMenuPlacesFailButton, stMenuPlacesFailRender },
  /* STATE_MENU_PLACES_CODE    */ { stMenuPlacesCodeEnter, stMenuPlacesCodeButton, stMenuPlacesCodeRender },
  /* STATE_MENU_WIFI_SSID      */ { stMenuWifiSsidEnter, nullptr, stMenuWifiSsidRender },
  /* STATE_MENU_WIFI_SSID_RES  */ { stMenuWifiSsidResEnter, stMenuWifiSsidResButton, stMenuWifiSsidResRender },
  /* STATE_MENU_WIFI_SSID_MANUAL */ { stMenuWifiSsidManualEnter, stMenuWifiSsidManualButton, stMenuWifiSsidManualRender },
  /* STATE_MENU_WIFI_PASS      */ { stMenuWifiPassEnter, stMenuWifiPassButton, stMenuWifiPassRender },
  /* STATE_MENU_WIFI_TEST      */ { stMenuWifiTestEnter, nullptr, stMenuWifiTestRender },
  /* STATE_MENU_WIFI_TEST_RUN  */ { stMenuWifiTestRunEnter, nullptr, nullptr },
  /* STATE_MENU_WIFI_TEST_OK   */ { stMenuWifiTestOkEnter, stMenuWifiTestOkButton, stMenuWifiTestOkRender },
  /* STATE_MENU_WIFI_TEST_FAIL */ { stMenuWifiTestFailEnter, stMenuWifiTestFailButton, stMenuWifiTestFailRender },
  /* STATE_MENU_HANDED         */ { stMenuHandedEnter, stMenuHandedButton, stMenuHandedRender },
  /* STATE_MENU_FACTORY        */ { stMenuFactoryEnter, stMenuFactoryButton, stMenuFactoryRender },
};

static_assert(sizeof(STATES) / sizeof(STATES[0]) == STATE_COUNT,
              "STATES must have exactly one row per AppState (in enum order)");

// --------------------------------------------------------------- LOOP

bool loopState() {
  if (state == stateNext) {
    return false;
  }

  LOG("main: switching state from %d to %d\n", state, stateNext);

  uint8_t lastStart = mainNavStartLast;
  uint8_t lastSelected = mainNavSelectedLast;
  mainNavStartLast = mainNavStart;
  mainNavSelectedLast = mainNavSelected;

  if (stateNext == statePrev) {
    mainNavStart = lastStart;
    mainNavSelected = lastSelected;
  } else {
    mainNavStart = 0;
    mainNavSelected = 0;
  }

  statePrev = state;
  state = stateNext;
  oledUpdate = true;

  if (state < STATE_COUNT && STATES[state].onEnter) {
    STATES[state].onEnter();
  } else {
    LOG("main: undefined state for state switch (%d)\n", state);
  }
  return true;
}

bool loopInteraction() {
  if (dialButtonGetAndClear()) {
    LOG("Button\n");
    if (state < STATE_COUNT && STATES[state].onButton) {
      STATES[state].onButton();
    } else {
      LOG("main: undefined state for button press (%d)\n", state);
    }
    LOG("main: next (%d)\n", stateNext);
    return true;
  }

  int8_t diff = dialPositionGetRelativeAndClear();
  if (diff != 0) {
    //LOG("Change %d %d\n", diff, mainNavSelected);
    oledUpdate |= mainNavDial(diff);
    return true;
  }

  return false;
}

bool loopOled() {
  if (!oledUpdate) {
    return false;
  }
  oledUpdate = false;

  oledBufferClear();

  if (state < STATE_COUNT && STATES[state].onRender) {
    STATES[state].onRender();
  } else {
    LOG("main: undefined state for oled update selected (%d)\n", state);
  }

  oledBufferSend();
  return true;
}

void enterSleep() {
  oledEnabled(false);

  // Sleep until the encoder GPIO wakes us — the only enabled wake source
  // (configured once in dialSetup). No timed wake is needed: the display is
  // off while asleep, so there is nothing to keep current, and the system
  // clock (RTC/LP domain) keeps advancing, so timeMinutesGet() is already
  // correct on wake. Loop in case some other source ever fires spuriously.
  do {
    esp_light_sleep_start();
  } while (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_GPIO);

  oledEnabled(true);

  dialButtonGetAndClear();
  dialPositionGetRelativeAndClear();

  oledUpdate = true;
  mainActionLast = time(NULL);
}

// --------------------------------------------------------------- ENTRY POINTS

void setup() {
  delay(500);

  LOG_BEGIN(115200);
  LOG("\n\n");
  LOG("%s %s %s\n", appName, appVersion, appRev);
  LOG("https://github.com/kiu/%s\n", appName);
  LOG("\n");

  oledSetup();

  oledShowSplash();
  delay(3000);

  wifiSetup();

  dialSetup();

  stateNext = STATE_PLACES_LIST;
}

void loop() {
  bool changed = false;
  changed |= loopState();
  changed |= loopInteraction();

  // Keep the displayed time current: the clock (RTC) advances on its own, so
  // redraw whenever the minute-of-day changes. This does not count as user
  // activity, so it never defers sleep (matching the old per-minute ISR).
  static uint32_t loopLastMinute = 0xFFFFFFFF;
  uint32_t nowMinute = timeMinutesGet();
  if (nowMinute != loopLastMinute) {
    loopLastMinute = nowMinute;
    mainOledUpdate();
  }

  loopOled();

  if (changed) {
    mainActionLast = time(NULL);
    return;
  }

#ifndef DEV_MODE
  if (SLEEP_IN_SECONDS > 0) {
    if ((time(NULL) - mainActionLast) > SLEEP_IN_SECONDS) {
      enterSleep();
      mainNetworkPlacesDownload();
    }
  }
#endif
}
