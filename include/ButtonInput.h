#pragma once
#include <Arduino.h>

struct ButtonEvents {
  bool pressed = false;
  bool released = false;
  bool tap = false;
  bool shortPress = false;
  bool doublePress = false;
  bool longPress = false;
};

class ButtonInput {
 public:
  explicit ButtonInput(uint8_t pin) : pin_(pin) {}
  void begin();
  ButtonEvents update(uint32_t now, uint16_t longPressMs, uint16_t doubleClickMs);
  bool isPressed() const { return stable_; }
  bool longPressTriggered() const { return longFired_; }
  uint32_t pressedFor(uint32_t now) const { return stable_ ? (now - pressStarted_) : 0; }

 private:
  uint8_t pin_;
  bool raw_ = false;
  bool stable_ = false;
  bool longFired_ = false;
  bool shortPending_ = false;
  uint32_t lastRawChange_ = 0;
  uint32_t pressStarted_ = 0;
  uint32_t shortDeadline_ = 0;
  static constexpr uint16_t DEBOUNCE_MS = 25;
};
