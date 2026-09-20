#include "SideTouchInput.h"

void SideTouchInput::begin() {
  pinMode(pin_, activeHigh_ ? INPUT_PULLDOWN : INPUT_PULLUP);
  const bool electrical = digitalRead(pin_) == HIGH;
  raw_ = stable_ = activeHigh_ ? electrical : !electrical;
}

TouchEvents SideTouchInput::update(uint32_t now) {
  TouchEvents ev;
  const bool electrical = digitalRead(pin_) == HIGH;
  const bool current = activeHigh_ ? electrical : !electrical;

  if (current != raw_) {
    raw_ = current;
    lastRawChange_ = now;
  }

  if ((now - lastRawChange_) >= DEBOUNCE_MS && stable_ != raw_) {
    stable_ = raw_;
    if (stable_) {
      touchStarted_ = now;
      ev.pressed = true;
    } else {
      ev.released = true;
    }
  }
  return ev;
}
