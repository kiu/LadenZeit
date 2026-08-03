#ifndef LZ_TIME_H
#define LZ_TIME_H

#include <Arduino.h>

// Backed by the ESP32 system clock (RTC/LP domain), which survives sleep.
uint8_t timeDayGet();      // weekday, 0 = Sunday .. 6 = Saturday
uint32_t timeMinutesGet(); // minutes-of-day, 0 .. 1439

void timeSet(uint32_t epoch);  // "wall-clock epoch": local time as secs-since-1970 (not true UTC)

void timeCacheInit();
bool timeCacheExpired();

#endif
