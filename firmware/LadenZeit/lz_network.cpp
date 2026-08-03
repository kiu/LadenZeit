#include "lz_network.h"

#include "lz_config.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiType.h>
#include <WiFiGeneric.h>
#include <Preferences.h>

#include "lz_places.h"

#include "esp_wifi.h"
#include "esp_bt.h"

Preferences networkPref;
const char* NETWORK_PREF = "network";
const char* NETWORK_PREF_DEVICE_ID = "device_id";
const char* NETWORK_PREF_WIFI_SSID = "wifi_ssid";
const char* NETWORK_PREF_WIFI_PASS = "wifi_pass";

const uint8_t WIFI_PASS_CHARS_LEN = 96;
const char* WIFI_PASS_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ^!\"$%&/{([])}=?\\`'~+-*/<|>;,:._#@";

const uint8_t WIFI_PASS_MAX_LEN = 32;
const uint8_t WIFI_PASS_CONTROLS = 3;  // Back, OK, Del — repeated at each end

const uint8_t HTTP_DEVICE_ID_LEN = 32;
String httpDeviceId;

const uint8_t HTTP_PASSCODE_LEN = 11;
String httpPasscodeHuman;

HTTPClient http;

uint8_t networkSsidsLen = 0;
String networkSsids[WIFI_SSID_MAX];

#ifndef DEV_MODE
const char* cfgApi = "https://iot.nakamura-labs.com/ladenzeit/api/v1/devices/";
WiFiClientSecure wifiClient;

const char* iotRootCa = R"rawliteral(
-----BEGIN CERTIFICATE-----
MIIEqDCCApACAQEwDQYJKoZIhvcNAQELBQAwGTEXMBUGA1UEAwwOSU9UIEV0ZXJu
YWwgQ0EwIBcNNzAwMTAxMDAwMDAwWhgPMjA3NjA1MDQxMjEwMDdaMBkxFzAVBgNV
BAMMDklPVCBFdGVybmFsIENBMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKC
AgEA0A+B2GCt2mcG9Y9ogoKTVUH1xF98R7WLiKA59JevXq/n8KYMoc95R3RXM74O
TI/+y3k8uHYqxuM7Na+Gc8iK/2hRs/o/wuUNvOBhWAlMa/V0LuU28oH5ArEbA34J
CdCsWRlN2YYATjQBkzR7bc//2sf0Sp9jiQiWy480G7j+bUAgK3nlNrg49V/8RFEn
6HzkG3zYTltMrZiYDlyr5Tw1rhQcIg986hCbkLAiuCqpUmpPuvrkMvkenzgZLBvZ
xVDm+iQP4x7EcbUoH6YXz0W5Ps72+skZJfOW5NeNqC69IfMgoGFfQaq4sJdo9ZsH
10cNPf7bpybDy+gO4B4vSJQv0WXhKLJC9hKi8swjg3/eW/BiNhsElSz1yoQ4HcAh
Xov4GclS72BerKxFHPhe+5PgBeEz97ljUEuDTb1iF8/Dn0NkKPJO2Tb2kXgGIwlH
wRn0JR07iF6LR+DYjZW92UPCxon/3pmjaE0DdDcP1XPbaGxGT499LQrq40jJDur8
KL27RKEkKbt6hcnISfAy+DyRqXYsQuSw/xLlV9S+Du1S5n2SrYxIqspAO904u0YZ
v1cxUSZPZanlK9/27PX+iHbBl4f1E9e1yNbpinmHf8HQ0OsrAYrBlSuwOSkQLYGZ
i7xhTw3tnR4AlSwzEfl+9ergfdP8asG8eqP1TtNNEh5E7xsCAwEAATANBgkqhkiG
9w0BAQsFAAOCAgEAaDfSD3IO2ZdlFc6b0J/AIdbcQCDO94mGkC8JLqTpxkM2yqUU
X91n3MgLZT03Msg24z3zKy/NtpKaY/kAdCvKspRt/W99FHu7jSjtC5g1vE5e+5tj
R/39oHEzgY8NCw4dm+r7x/XfOn1Uxx09x3JsVqipUflnOdLZTfHWHdcKwk1quyI8
MFiadYWxmtW/rD5xNJRZqEepNo4EedjwGpWOblxJ3TuQdR5fp+qRpLw5YUMU1C5f
EWNFrOqvzbFhupCnuLdJRNYeHjLEcT/Bas8y4khT8vDPV6W9sjWYIiMtkvaeZmIA
X0PQrrhALla7BQX6kP5AnhmQsqSF43zPvTI2a0VqDCcsQbHzL3ulN+6AoGzFsWE/
1Q1lu3czGOvAAm5aiKfeK7oeatNfCsFIWdW62KbtGtSi533uqhpE3Au8jpSZ9Auu
r6kxJ3r05jGlHTprP1OX1nf0qcq9roW3XK+hpfNMTmFFBTg/obuP72wUr9JdzHA1
xDZPi/cMcrHwXfu4aMK1lZ1Hfd/k5atN4ZdnJgqZnpTuqn4LqCs0y/1t2icSFOON
bgFyg+1N9k0zoVJr6JAGdmdnDIsbN2trfl55e7jfntkil9mjGg+96lH8tpVdd5Fu
Xbcvw/qWdZy0r8yYuAaBwa+vew84gZLe+fwBDwcBn4Y1rlm2SjuEHmTmrdc=
-----END CERTIFICATE-----
)rawliteral";
#else
const char* cfgApi = "http://machariel.qnet:8080/api/v1/devices/";
#endif

uint8_t wifiPassSlotCount() {
  return WIFI_PASS_CHARS_LEN + 2 * WIFI_PASS_CONTROLS;
}

WifiPassKey wifiPassSlotKey(uint8_t slot, char* outChar) {
  uint8_t last = wifiPassSlotCount() - 1;
  if (slot == 0 || slot == last) {
    return WIFI_PASS_KEY_BACK;
  }
  if (slot == 1 || slot == last - 1) {
    return WIFI_PASS_KEY_OK;
  }
  if (slot == 2 || slot == last - 2) {
    return WIFI_PASS_KEY_DEL;
  }
  if (outChar) {
    *outChar = WIFI_PASS_CHARS[slot - WIFI_PASS_CONTROLS];
  }
  return WIFI_PASS_KEY_CHAR;
}

String httpGenerateCode(uint8_t len, const String& charset) {
  String result;
  result.reserve(len);

  uint8_t charsetLen = charset.length();
  for (uint8_t i = 0; i < len; i++) {
    result.concat(charset[esp_random() % charsetLen]);
  }
  return result;
}

void disableOtherRadios() {
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
}

void wifiSetup() {
#ifndef DEV_MODE
  wifiClient.setCACert(iotRootCa);
#endif
  disableOtherRadios();

  networkPref.begin(NETWORK_PREF, false);
  httpDeviceId = networkPref.getString(NETWORK_PREF_DEVICE_ID, "");
#ifdef DEV_DEVICE_ID
  httpDeviceId = DEV_DEVICE_ID;
#endif

  LOG("wifi: Device ID: %s\n", httpDeviceId.c_str());
  if (httpDeviceId.length() != HTTP_DEVICE_ID_LEN) {
    httpDeviceId = httpGenerateCode(HTTP_DEVICE_ID_LEN, "0123456789ABCDEF");
    networkPref.putString(NETWORK_PREF_DEVICE_ID, httpDeviceId);
  }

  LOG("wifi: Device ID: %s\n", httpDeviceId.c_str());
  networkPref.end();

  WiFi.disconnect(true, true);
}

uint8_t wifiScanLen() {
  return networkSsidsLen;
}

String* wifiScanItems() {
  return networkSsids;
}

uint8_t wifiScan() {
  wifiDisconnect();

  LOG("wifi: scanning for networks\n");
  uint8_t ssidsLen = WiFi.scanNetworks(false, true, false, 600, 0, nullptr, nullptr);

  LOG("wifi: Found %d networks\n", ssidsLen);
  if (ssidsLen > WIFI_SSID_MAX) {
    ssidsLen = WIFI_SSID_MAX;
  }
  networkSsidsLen = ssidsLen;

  for (uint8_t i = 0; i < ssidsLen; i++) {
    LOG("wifi:   [%02d] %s\n", i, WiFi.SSID(i).c_str());
    networkSsids[i] = WiFi.SSID(i).c_str();
  }

  return ssidsLen;
}

bool wifiConnectCredentialsPower(const String& ssid, const String& pass, wifi_power_t power, const String& powerName) {
  LOG("wifi: connecting to %s (Power: %d)\n", ssid.c_str(), power);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(500);  // Give the network stack a moment to reset

  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

  WiFi.setTxPower(power);
  WiFi.begin(ssid, pass);
  uint32_t start = millis();
  while (millis() - start < (1000 * 7)) {
    if (WiFi.isConnected()) {
      LOG("wifi: connected (Power: %s, Device IP: %s)\n", powerName.c_str(), WiFi.localIP().toString().c_str());
      return true;
    }
    delay(500);
  }

  return false;
}

NetworkState wifiConnectCredentials(const String& ssid, const String& pass) {
  wifiDisconnect();

  if (ssid == "") {
    LOG("wifi: ssid and/or pass not setup\n");
    return NETWORK_ERROR_CONFIGURATION;
  }

  bool success;
  success = wifiConnectCredentialsPower(ssid, pass, WIFI_POWER_7dBm, "Low");
  if (success) {
    return NETWORK_OK;
  }
  success = wifiConnectCredentialsPower(ssid, pass, WIFI_POWER_13dBm, "Medium");
  if (success) {
    return NETWORK_OK;
  }
  success = wifiConnectCredentialsPower(ssid, pass, WIFI_POWER_19dBm, "High");
  if (success) {
    return NETWORK_OK;
  }

  LOG("wifi: failed to connect\n");
  wifiDisconnect();
  return NETWORK_ERROR_CONNECTION;
}

NetworkState wifiConnectAndStore(const String& ssid, const String& pass) {
  NetworkState result = wifiConnectCredentials(ssid, pass);
  if (result == NETWORK_OK) {
    networkPref.begin(NETWORK_PREF, false);
    networkPref.putString(NETWORK_PREF_WIFI_SSID, ssid);
    networkPref.putString(NETWORK_PREF_WIFI_PASS, pass);
    networkPref.end();
  }
  return result;
}

NetworkState wifiConnect() {
  networkPref.begin(NETWORK_PREF, true);
  String ssid = networkPref.getString(NETWORK_PREF_WIFI_SSID, "");
  String pass = networkPref.getString(NETWORK_PREF_WIFI_PASS, "");
  networkPref.end();

#ifdef DEV_WIFI_SSID
  ssid = DEV_WIFI_SSID;
  pass = DEV_WIFI_PASS;
#endif

  return wifiConnectCredentials(ssid, pass);
}

void wifiDisconnect() {
  LOG("wifi: disconnecting\n");
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  //esp_wifi_stop();
  LOG("wifi: disconnected\n");
}

NetworkState httpDownloadPlaces() {
  String url = String(cfgApi);
  url.concat(httpDeviceId);
  url.concat("/places");

  LOG("http: download %s\n", url.c_str());

  if (!WiFi.isConnected()) {
    LOG("http: failed, not connected\n");
    return NETWORK_ERROR_CONNECTION;
  }

#ifndef DEV_MODE
  http.begin(wifiClient, url);
#else
  http.begin(url);
#endif

  http.setTimeout(30000);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    LOG("http: failed, not OK (%d)\n", httpCode);
    return NETWORK_ERROR_DATA;
  }
  LOG("http: OK (%d)\n", httpCode);

  int len = http.getSize();
  if (len < placesBufferMin()) {
    http.end();
    LOG("http: failed, expects minimum %d bytes, got %d\n", placesBufferMin(), len);
    return NETWORK_ERROR_DATA;
  }
  if (len > placesBufferMax()) {
    http.end();
    LOG("http: failed, expects maximum %d bytes, got %d\n", placesBufferMax(), len);
    return NETWORK_ERROR_DATA;
  }
  LOG("http: length %d bytes\n", len);

  WiFiClient* stream = http.getStreamPtr();
  int readBytes = stream->readBytes(placesBufferPtr(), len);
  if (readBytes != len) {
    http.end();
    LOG("http: failed, actually read %d bytes\n", readBytes);
    placesReset();
    return NETWORK_ERROR_DATA;
  }
  LOG("http: read %d bytes\n", readBytes);

  if (!placesValidate((uint16_t)len)) {
    http.end();
    LOG("http: failed, malformed places data\n");
    placesDumpRaw((uint16_t)len);
    placesReset();
    return NETWORK_ERROR_DATA;
  }

  http.end();
  LOG("http: success\n");
  return NETWORK_OK;
}

NetworkState httpEnableConfig() {
  String url = String(cfgApi);
  url.concat(httpDeviceId);
  url.concat("/auth/otp");

  LOG("http: post %s\n", url.c_str());

  if (!WiFi.isConnected()) {
    LOG("http: failed, not connected\n");
    return NETWORK_ERROR_CONNECTION;
  }

#ifndef DEV_MODE
  http.begin(wifiClient, url);
#else
  http.begin(url);
#endif

  http.addHeader("Content-Type", "application/json");
  int httpCode = http.sendRequest("PATCH", "{}");

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    LOG("http: failed, not OK (%d)\n", httpCode);
    return NETWORK_ERROR_DATA;
  }
  LOG("http: OK (%d)\n", httpCode);

  int len = http.getSize();
  if (len != HTTP_PASSCODE_LEN) {
    http.end();
    LOG("http: failed, expects exact %d bytes, got %d\n", HTTP_PASSCODE_LEN, len);
    return NETWORK_ERROR_DATA;
  }
  LOG("http: length %d bytes\n", len);

  char code[HTTP_PASSCODE_LEN + 1];

  WiFiClient* stream = http.getStreamPtr();
  int readBytes = stream->readBytes(code, len);
  if (readBytes != len) {
    http.end();
    LOG("http: failed, actually read %d bytes\n", readBytes);
    return NETWORK_ERROR_DATA;
  }
  LOG("http: read %d bytes\n", readBytes);
  code[len] = '\0';  // readBytes doesn't terminate; code[] is sized len+1 for this

  httpPasscodeHuman = code;
  LOG("http: OTP code: %s\n", code);

  http.end();
  LOG("http: success\n");
  return NETWORK_OK;
}