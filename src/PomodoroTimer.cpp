#include "PomodoroTimer.h"

void PomodoroTimer::start(uint16_t minutes, bool isBreak) {
  totalMs_ = static_cast<uint32_t>(minutes) * 60000UL;
  remainingMs_ = totalMs_;
  isBreak_ = isBreak;
  running_ = true;
  paused_ = false;
  lastUpdate_ = millis();
}

bool PomodoroTimer::update(uint32_t now) {
  if (!running_) return false;
  if (paused_) {
    lastUpdate_ = now;
    return false;
  }

  uint32_t delta = now - lastUpdate_;
  lastUpdate_ = now;
  if (delta >= remainingMs_) {
    remainingMs_ = 0;
    running_ = false;
    return true;
  }
  remainingMs_ -= delta;
  return false;
}

void PomodoroTimer::togglePause(uint32_t now) {
  if (!running_) return;
  paused_ = !paused_;
  lastUpdate_ = now;
}

void PomodoroTimer::cancel() {
  running_ = false;
  paused_ = false;
  remainingMs_ = 0;
}
