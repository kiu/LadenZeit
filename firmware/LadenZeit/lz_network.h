#ifndef LZ_NETWORK_H
#define LZ_NETWORK_H

#include <Arduino.h>

// NETWORK_PROD is defined (or not) in lz_secrets.h — see lz_secrets.example.h.

#define WIFI_SSID_LEN 32  // max SSID text length (chars)
#define WIFI_SSID_MAX 16  // max scanned SSIDs kept (networkSsids[] size)

extern const uint8_t WIFI_PASS_CHARS_LEN;
extern const char* WIFI_PASS_CHARS;

extern const uint8_t WIFI_PASS_MAX_LEN;  // max length of an entered password

// The pass-entry screen is a scrollable keyboard laid out as
//   [Back][OK][Del]  <charset...>  [Del][OK][Back]
// (the three controls are repeated, mirrored, at each end so the dial reaches
// them from either side). wifiPassSlotKey() classifies a slot index so the
// button handler and the renderer share one source of truth for the layout.
enum WifiPassKey {
  WIFI_PASS_KEY_BACK,
  WIFI_PASS_KEY_OK,
  WIFI_PASS_KEY_DEL,
  WIFI_PASS_KEY_CHAR,
};

uint8_t wifiPassSlotCount();                          // total selectable slots
WifiPassKey wifiPassSlotKey(uint8_t slot, char* outChar);  // outChar set for CHAR

extern const uint8_t HTTP_PASSCODE_LEN;
extern String httpPasscodeHuman;

enum NetworkState {
  NETWORK_OK,
  NETWORK_ERROR_CONFIGURATION,
  NETWORK_ERROR_CONNECTION,
  NETWORK_ERROR_DATA,
};

void wifiSetup();

uint8_t wifiScan();
uint8_t wifiScanLen();
String* wifiScanItems();

NetworkState wifiConnect();
NetworkState wifiConnectAndStore(const String& ssid, const String& pass);
NetworkState httpDownloadPlaces();
NetworkState httpEnableConfig();
void wifiDisconnect();

#endif