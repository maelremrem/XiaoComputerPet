#include "ButtonInput.h"

void ButtonInput::begin() {
  pinMode(pin_, INPUT_PULLUP);
  raw_ = stable_ = (digitalRead(pin_) == LOW);
}

ButtonEvents ButtonInput::update(uint32_t now, uint16_t longPressMs, uint16_t doubleClickMs) {
  ButtonEvents ev;
  const bool current = (digitalRead(pin_) == LOW);

  if (current != raw_) {
    raw_ = current;
    lastRawChange_ = now;
  }

  if ((now - lastRawChange_) >= DEBOUNCE_MS && stable_ != raw_) {
    stable_ = raw_;
    if (stable_) {
      pressStarted_ = now;
      longFired_ = false;
      ev.pressed = true;
    } else {
      ev.released = true;
      if (!longFired_) {
        if (shortPending_ && static_cast<int32_t>(now - shortDeadline_) <= 0) {
          shortPending_ = false;
          ev.doublePress = true;
        } else {
          // Immediate first-tap feedback. shortPress is still emitted later,
          // once the double-click window has safely expired.
          ev.tap = true;
          shortPending_ = true;
          shortDeadline_ = now + doubleClickMs;
        }
      }
    }
  }

  if (stable_ && !longFired_ && (now - pressStarted_) >= longPressMs) {
    longFired_ = true;
    shortPending_ = false;
    ev.longPress = true;
  }

  if (shortPending_ && !stable_ && static_cast<int32_t>(now - shortDeadline_) >= 0) {
    shortPending_ = false;
    ev.shortPress = true;
  }

  return ev;
}
