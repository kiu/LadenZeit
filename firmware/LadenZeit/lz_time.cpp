#include "lz_time.h"

#include "lz_config.h"

#include <ESP32Time.h>

// The ESP32-C6 system clock lives in the RTC/LP power domain: it keeps
// advancing through light sleep, so it is the single source of truth for time.
ESP32Time rtc(0);  // offset 0 seconds

// Epoch (seconds) at which the downloaded places cache is considered stale.
// 0 = never fetched, so timeCacheExpired() reports expired until the first
// successful download seeds the clock and calls timeCacheInit().
uint32_t timeCacheExpiry = 0;
const uint32_t TIME_CACHE_TTL_SEC = 3600 * 12;

uint8_t timeDayGet() {
  return rtc.getDayofWeek();  // 0 = Sunday .. 6 = Saturday
}

uint32_t timeMinutesGet() {
  return (uint32_t)rtc.getHour(true) * 60 + rtc.getMinute();  // minutes-of-day
}

void timeSet(uint32_t epoch) {
  LOG("time: setting RTC for epoch: %lu\n", (unsigned long)epoch);

  // The network payload carries a "wall-clock epoch": the server's LOCAL time
  // encoded as seconds-since-1970 (real UTC epoch + timezone/DST offset), NOT a
  // true UTC epoch. rtc has offset 0, so it's interpreted verbatim and
  // getDayofWeek()/getHour()/getMinute() read back the intended local calendar
  // values. Keeping the offset on the server means the device needs no timezone
  // or DST rules; the trade-off is the value is only correct until the next
  // fetch after a DST change.
  rtc.setTime((unsigned long)epoch, 0);

  LOG("time: RTC is now: %s\n", rtc.getTime("%A %Y-%m-%d %H:%M:%S").c_str());
}

void timeCacheInit() {
  timeCacheExpiry = rtc.getEpoch() + TIME_CACHE_TTL_SEC;
}
bool timeCacheExpired() {
  return rtc.getEpoch() >= timeCacheExpiry;
}
