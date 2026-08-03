#ifndef LZ_CONFIG_H
#define LZ_CONFIG_H

#include <Arduino.h>

// Pull in local development settings if present (lz_secrets.h is gitignored).
// A checkout without that file builds as a production image.
#if defined(__has_include)
#  if __has_include("lz_secrets.h")
#    include "lz_secrets.h"
#  endif
#endif

// DEV_MODE is the single development toggle (defined in lz_secrets.h). When set:
//   * hardcoded WiFi credentials are used        (see lz_network.cpp)
//   * the hardcoded device id is used            (see lz_network.cpp)
//   * the development endpoint is used           (see lz_network.cpp)
//   * the device never enters sleep              (see LadenZeit.ino loop())
//   * serial debug logging is emitted            (LOG* macros below)
// In production (DEV_MODE undefined) the LOG* macros compile to nothing, so the
// image emits zero serial output.
#ifdef DEV_MODE
#  define LOG_BEGIN(baud) Serial.begin(baud)
#  define LOG(...)        Serial.printf(__VA_ARGS__)
#  define LOG_FLUSH()     Serial.flush()
#else
#  define LOG_BEGIN(baud) ((void)0)
#  define LOG(...)        ((void)0)
#  define LOG_FLUSH()     ((void)0)
#endif

#endif
