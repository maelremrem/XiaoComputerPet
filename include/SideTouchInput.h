#pragma once
#include <Arduino.h>

struct TouchEvents {
  bool pressed = false;
  bool released = false;
};

class SideTouchInput {
 public:
  SideTouchInput(uint8_t pin, bool activeHigh = true) : pin_(pin), activeHigh_(activeHigh) {}
  void begin();
  TouchEvents update(uint32_t now);
  bool isTouched() const { return stable_; }
  uint32_t touchedFor(uint32_t now) const { return stable_ ? (now - touchStarted_) : 0; }

 private:
  uint8_t pin_;
  bool activeHigh_;
  bool raw_ = false;
  bool stable_ = false;
  uint32_t lastRawChange_ = 0;
  uint32_t touchStarted_ = 0;
  static constexpr uint16_t DEBOUNCE_MS = 18;
};
