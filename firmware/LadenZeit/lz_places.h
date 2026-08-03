#ifndef LZ_PLACES_H
#define LZ_PLACES_H

#include <Arduino.h>

#define PLACES_NAME_MAX 32  // stack buffer size for placesName()

// Slot times are week-minutes: day * MINUTES_PER_DAY + hour * 60 + min, 0 = Sunday.
#define DAYS_PER_WEEK 7
#define MINUTES_PER_DAY 1440
#define MINUTES_PER_WEEK 10080

uint16_t placesBufferMax();
uint16_t placesBufferMin();
uint8_t* placesBufferPtr();

void placesUpdateTime();
uint8_t placesCount();

void placesName(uint8_t pidx, char* out, uint8_t cap);

// Open now? Also outputs minutes until the next status change (close if open,
// else open).
bool placesStatus(uint8_t pidx, uint16_t* minsUntilChange);

void placesReset();
bool placesValid();
bool placesValidate(uint16_t len);

void placesDebug();
void placesDumpRaw(uint16_t len);  // dev-only hex dump of the raw buffer

uint8_t placesSlotCount(uint8_t pidx);
uint32_t placesSlot(uint8_t pidx, uint8_t sidx);
uint16_t placesSlotFrom(uint32_t slot);  // opening week-minute
uint16_t placesSlotTo(uint32_t slot);    // closing week-minute


#endif