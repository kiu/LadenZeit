#include "lz_dial.h"

#include <esp_sleep.h>

volatile int8_t encoderPos = 0;
volatile uint8_t encoderCodeLast = 0;

volatile bool encoderButtonPress = false;  // debounced press, consumed by dialButtonGetAndClear()
volatile unsigned long encoderButtonLastBounceTime = 0;
const unsigned long encoderButtonDebounceDelay = 25;

void IRAM_ATTR dialInterruptRotate() {
  uint8_t MSB = digitalRead(PIN_ENCODER_MSB);
  uint8_t LSB = digitalRead(PIN_ENCODER_LSB);
  uint8_t encoded = (MSB << 1) | LSB;
  uint8_t sum = (encoderCodeLast << 2) | encoded;
  encoderCodeLast = encoded;
  // The standard quadrature sequence is 00 -> 01 -> 11 -> 10 -> 00
  if (encoderPos < 127 && (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011)) encoderPos++;
  if (encoderPos > -127 && (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000)) encoderPos--;
}

// Button is active-low (pressed = LOW), attached on CHANGE so this runs on every
// edge. Edges closer together than encoderButtonDebounceDelay are contact
// chatter: keep extending the quiet window and ignore them. Once the signal has
// been quiet for the full window, a press is registered on the settled release —
// the edge at which the pin reads HIGH again. Sampling only settled HIGH levels
// (never mid-bounce) is what makes repeated clicks reliable.
void IRAM_ATTR dialInterruptButton() {
  unsigned long now = millis();

  if (now - encoderButtonLastBounceTime < encoderButtonDebounceDelay) {
    encoderButtonLastBounceTime = now;  // chatter: extend the quiet window
    return;
  }

  if (digitalRead(PIN_ENCODER_BTN) == HIGH) {  // settled release
    encoderButtonLastBounceTime = now;
    encoderButtonPress = true;
  }
}

void dialSetup() {
  pinMode(PIN_ENCODER_MSB, INPUT);
  pinMode(PIN_ENCODER_LSB, INPUT);
  pinMode(PIN_ENCODER_BTN, INPUT);

  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_MSB), dialInterruptRotate, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_LSB), dialInterruptRotate, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_BTN), dialInterruptButton, CHANGE);

  gpio_wakeup_enable((gpio_num_t)PIN_ENCODER_MSB, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)PIN_ENCODER_LSB, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)PIN_ENCODER_BTN, GPIO_INTR_LOW_LEVEL);

  esp_sleep_enable_gpio_wakeup();
}

bool dialButtonGetAndClear() {
  bool result = encoderButtonPress;
  encoderButtonPress = false;
  return result;
}

int8_t dialPositionGetRelativeAndClear() {
  int8_t result = encoderPos / 4;
  if (result != 0) {
    encoderPos = 0;
  }
  return result;
}