#ifndef LZ_DIAL_H
#define LZ_DIAL_H

#include <Arduino.h>

#define PIN_ENCODER_MSB 3
#define PIN_ENCODER_LSB 1
#define PIN_ENCODER_BTN 0

void dialSetup();

bool dialButtonGetAndClear();

int8_t dialPositionGetRelativeAndClear();

#endif