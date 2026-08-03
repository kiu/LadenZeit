#ifndef LZ_OLED_H
#define LZ_OLED_H

#include <Arduino.h>

#define PIN_OLED_CLK 4
#define PIN_OLED_MOSI 6
#define PIN_OLED_CS 2
#define PIN_OLED_DC 10
#define PIN_OLED_RES 7

// Selectable cells visible per screen — drives both nav paging (mainNavPerPage
// in the state handlers) and the matching render loops in lz_oled.cpp.
#define PLACES_PER_PAGE 4
#define MENU_PER_PAGE 4
#define WIFI_SSID_PER_PAGE 2
#define WIFI_PASS_PER_PAGE 16
#define DETAIL_DAYS_PER_PAGE 2

void oledSetup();
void oledBufferClear();
void oledBufferSend();
void oledEnabled(bool enabled);
uint8_t oledFlipGet();
void oledFlipSet(bool flip);

void oledShowSplash();

void oledShowWifiSsid(uint8_t navStart, uint8_t selected);
void oledShowWifiSsidManual(const char* ssid, uint8_t navStart, uint8_t selected);
void oledShowWifiPass(const char* ssid, const char* pass, uint8_t navStart, uint8_t selected);

void oledShowPlacesList(uint8_t navStart, uint8_t entries, uint8_t selected);
void oledShowPlacesListError(const char* text1, const char* text2, uint8_t selected);
void oledShowPlacesDetail(uint8_t pidx, uint8_t navStart, uint8_t selected);

void oledShowMenu(uint8_t navStart, uint8_t selected);

void oledShowDialog(const char* head, const char* msg, uint8_t btnCount, const char* btns[], uint8_t selected);

void oledShowConnectionStatus(uint8_t wifi, uint8_t data);

#endif