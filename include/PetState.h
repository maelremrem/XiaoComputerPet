#pragma once
#include <Arduino.h>

struct PetState {
  uint16_t version = 3;
  uint8_t mood = 72;
  uint8_t energy = 82;
  uint8_t affection = 55;
  uint8_t boredom = 0; // Deprecated compatibility field; no attention debt is tracked.

  uint32_t xp = 0;
  uint16_t level = 1;
  uint32_t pets = 0;
  uint32_t boops = 0;
  uint32_t focusSessions = 0;
  uint32_t focusMinutes = 0;
  uint32_t focusXp = 0;
  uint32_t hugs = 0;
  uint32_t scratches = 0;
  uint32_t swipes = 0;
  uint32_t rareEvents = 0;

  uint16_t streakDays = 0;
  int32_t lastFocusDay = -1;
  int32_t todayDay = -1;
  uint16_t todaySessions = 0;
  uint16_t todayMinutes = 0;
};
