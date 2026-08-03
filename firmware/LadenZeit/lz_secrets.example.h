#ifndef LZ_SECRETS_H
#define LZ_SECRETS_H

// Copy this file to "lz_secrets.h" (which is gitignored) and edit it.
//
// DEV_MODE is the single development toggle. When it is defined the firmware:
//   * uses the hardcoded WiFi credentials below
//   * uses the hardcoded device id below
//   * talks to the development endpoint (plain HTTP)
//   * never enters sleep
//   * emits serial debug logging
// Comment DEV_MODE out (or leave lz_secrets.h absent) to build a production
// image: real WiFi provisioning + generated device id + HTTPS, and zero serial
// output.

#define DEV_MODE 1

#ifdef DEV_MODE
#  define DEV_WIFI_SSID "your-ssid"
#  define DEV_WIFI_PASS "your-pass"
#  define DEV_DEVICE_ID "00000000000000000000000000000000"
#endif

#endif
