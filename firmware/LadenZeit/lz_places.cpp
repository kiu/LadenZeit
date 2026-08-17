#include "lz_places.h"

#include "lz_config.h"
#include "lz_time.h"

// Payload header: 4-byte epoch + 1-byte place count. Per-place data begins at
// this offset, and it is also the smallest valid payload (header + zero places).
const uint16_t PLACES_HEADER_LEN = 5;
// Each slot is two big-endian uint16s (from, to) = 4 bytes.
const uint16_t PLACES_SLOT_LEN = 4;
const uint16_t PLACES_BUFFER_MIN = PLACES_HEADER_LEN;
const uint16_t PLACES_BUFFER_MAX = 1024;
uint8_t placesBuffer[PLACES_BUFFER_MAX] = { 0 };

// Number of payload bytes proven valid by placesValidate(). 0 = no data.
uint16_t placesBufferLen = 0;

uint16_t placesBufferMin() {
  return PLACES_BUFFER_MIN;
}
uint16_t placesBufferMax() {
  return PLACES_BUFFER_MAX;
}
uint8_t* placesBufferPtr() {
  return placesBuffer;
}
void placesReset() {
  for (uint8_t i = 0; i < PLACES_HEADER_LEN; i++) {
    placesBuffer[i] = 0;
  }
  placesBufferLen = 0;
}

// True once a download has passed placesValidate() and not since been reset.
// Distinguishes "have data" from an empty buffer independently of the cache
// timer (a rejected refresh clears the buffer but not the timer).
bool placesValid() {
  return placesBufferLen > 0;
}

// Verify the whole buffer is well-formed within the first `len` bytes before any
// accessor is allowed to trust it. Mirrors the placesIndex() walk but bounds
// every step. Comparisons are written as `need > len - pos` (with pos < len
// already established) to avoid uint16_t overflow. On success records
// placesBufferLen; on failure the caller is expected to placesReset().
bool placesValidate(uint16_t len) {
  if (len < PLACES_HEADER_LEN) {
    LOG("places: Verification failed. Data too short: %d\n", len);
    return false;  // epoch (4) + place count (1) must fit
  }

  uint8_t count = placesBuffer[PLACES_HEADER_LEN - 1];
  uint16_t pos = PLACES_HEADER_LEN;

  for (uint8_t i = 0; i < count; i++) {
    // name: 1 length byte + nameLen bytes
    if (pos >= len) {
      LOG("places: Verification failed. Name start out of bounds: %d\n", pos);
      return false;
    }
    uint8_t nameLen = placesBuffer[pos];
    if ((uint16_t)nameLen + 1 > len - pos) {
      LOG("places: Verification failed. Name out of bounds: %d %d\n", pos, nameLen + 1);
      return false;
    }
    pos += nameLen + 1;  // now at the slot-count byte

    // slots: 1 count byte + slotCount * 4 bytes
    if (pos >= len) {
      LOG("places: Verification failed. Slot start out of bounds: %d\n", pos);
      return false;
    }
    uint8_t slotCount = placesBuffer[pos];
    if ((uint32_t)slotCount * PLACES_SLOT_LEN + 1 > (uint32_t)(len - pos)) {
      LOG("places: Verification failed. Slot out of bounds: %d %d\n", pos, (slotCount * PLACES_SLOT_LEN + 1));
      return false;
    }
    pos += slotCount * PLACES_SLOT_LEN + 1;
  }

  placesBufferLen = len;
  return true;
}

void placesUpdateTime() {
  // Bytes [0..3]: big-endian uint32 wall-clock epoch (server-local time as
  // seconds-since-1970, i.e. real epoch + tz/DST offset — see timeSet()).
  uint32_t epoch = ((uint32_t)placesBuffer[0] << 24) |
                   ((uint32_t)placesBuffer[1] << 16) |
                   ((uint32_t)placesBuffer[2] << 8) |
                   ((uint32_t)placesBuffer[3]);
  timeSet(epoch);
}

uint8_t placesCount() {
  return placesBuffer[PLACES_HEADER_LEN - 1];  // count byte, last header byte
}

uint16_t placesIndex(uint8_t pidx) {
  uint16_t pos = PLACES_HEADER_LEN;
  for (uint8_t i = 0; i < placesCount(); i++) {
    if (i == pidx) {
      return pos;
    }
    pos += placesBuffer[pos] + 1;  // add name length
    pos += placesBuffer[pos] * 4;  // add slots
    pos += 1;                      // slot count
  }
  return pos;
}
void placesName(uint8_t pidx, char* out, uint8_t cap) {
  uint16_t pos = placesIndex(pidx);
  uint8_t len = placesBuffer[pos];
  if (len > cap - 1) {
    len = cap - 1;
  }
  for (uint8_t i = 0; i < len; i++) {
    out[i] = (char)placesBuffer[pos + 1 + i];
  }
  out[len] = '\0';
}

uint8_t placesSlotCount(uint8_t pidx) {
  uint16_t pos = placesIndex(pidx);
  pos += placesBuffer[pos] + 1;  // add name length
  return placesBuffer[pos];
}

uint32_t placesSlot(uint8_t pidx, uint8_t sidx) {
  uint16_t pos = placesIndex(pidx);
  pos += placesBuffer[pos] + 1;  // add name length
  pos += 1;                      // count
  pos += sidx * PLACES_SLOT_LEN;

  uint32_t result = 0;
  result = (result << 8) | placesBuffer[pos++];
  result = (result << 8) | placesBuffer[pos++];
  result = (result << 8) | placesBuffer[pos++];
  result = (result << 8) | placesBuffer[pos++];
  return result;
}

// A slot is two big-endian uint16 week-minutes (day*1440 + hour*60 + min,
// 0 = Sunday): opening time in the high 16 bits, closing time in the low 16.
uint16_t placesSlotFrom(uint32_t slot) {
  return (slot >> 16) & 0xFFFF;
}
uint16_t placesSlotTo(uint32_t slot) {
  return slot & 0xFFFF;
}

// Open/closed now, plus minutes until the next status change (until the next
// close if open, else until the next open). Slots are sorted and non-overlapping;
// each may cross midnight or the Sat->Sun week boundary (to < from), so all "time
// until" math is done modulo the week. 24/7 places (to == from) are filtered out
// server-side.
bool placesStatus(uint8_t pidx, uint16_t* minsUntilChange) {
  uint16_t now = timeDayGet() * MINUTES_PER_DAY + timeMinutesGet();

  bool open = false;
  uint16_t nextOpenDist = MINUTES_PER_WEEK;   // minutes until the next opening
  uint16_t nextCloseDist = MINUTES_PER_WEEK;  // minutes until the next closing

  for (uint8_t s = 0; s < placesSlotCount(pidx); s++) {
    uint32_t slot = placesSlot(pidx, s);
    uint16_t from = placesSlotFrom(slot);
    uint16_t to = placesSlotTo(slot);

    if (to > from) {
      open |= (now >= from && now < to);
    } else if (to < from) {
      open |= (now >= from || now < to);  // wraps the week boundary
    }

    // Circular distance from now to each boundary; keep the nearest of each.
    uint16_t df = ((uint32_t)from + MINUTES_PER_WEEK - now) % MINUTES_PER_WEEK;
    if (df < nextOpenDist) {
      nextOpenDist = df;
    }
    uint16_t dt = ((uint32_t)to + MINUTES_PER_WEEK - now) % MINUTES_PER_WEEK;
    if (dt < nextCloseDist) {
      nextCloseDist = dt;
    }
  }

  *minsUntilChange = open ? nextCloseDist : nextOpenDist;
  return open;
}

// Raw hex/ASCII dump of the first `len` received bytes, for diagnosing a
// payload the validator rejected. Dev-only (LOG compiles away in production).
void placesDumpRaw(uint16_t len) {
  #ifdef DEV_MODE
  if (len > PLACES_BUFFER_MAX) {
    len = PLACES_BUFFER_MAX;
  }
  LOG("places: raw %d bytes\n", len);
  for (uint16_t row = 0; row < len; row += 16) {
    char line[80];
    int n = snprintf(line, sizeof line, "places: %04X ", row);
    for (uint8_t i = 0; i < 16 && row + i < len; i++) {
      n += snprintf(line + n, sizeof line - n, "%02X ", placesBuffer[row + i]);
    }
    n += snprintf(line + n, sizeof line - n, " |");
    for (uint8_t i = 0; i < 16 && row + i < len; i++) {
      uint8_t c = placesBuffer[row + i];
      n += snprintf(line + n, sizeof line - n, "%c", (c >= 32 && c < 127) ? c : '.');
    }
    LOG("%s|\n", line);
  }
  #endif
}

void placesDebug() {
  #ifdef DEV_MODE
  LOG("places: %d places\n", placesCount());
  for (uint8_t i = 0; i < placesCount(); i++) {
    char name[PLACES_NAME_MAX];
    placesName(i, name, sizeof name);
    LOG("places: [%03d] '%s' (%d slots)\n", i, name, placesSlotCount(i));
    for (uint8_t s = 0; s < placesSlotCount(i); s++) {
      uint32_t slot = placesSlot(i, s);
      uint16_t from = placesSlotFrom(slot);
      uint16_t to = placesSlotTo(slot);
      LOG("places:   [%03d] d%d %02d:%02d - d%d %02d:%02d\n", s,
          from / MINUTES_PER_DAY, (from % MINUTES_PER_DAY) / 60, from % 60,
          to / MINUTES_PER_DAY, (to % MINUTES_PER_DAY) / 60, to % 60);
    }
  }
  #endif
}