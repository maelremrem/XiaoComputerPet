#pragma once
#include <Arduino.h>

class PomodoroTimer {
 public:
  void start(uint16_t minutes, bool isBreak = false);
  bool update(uint32_t now);
  void togglePause(uint32_t now);
  void cancel();

  bool running() const { return running_; }
  bool paused() const { return paused_; }
  bool isBreak() const { return isBreak_; }
  uint32_t remainingMs() const { return remainingMs_; }
  uint32_t totalMs() const { return totalMs_; }

 private:
  bool running_ = false;
  bool paused_ = false;
  bool isBreak_ = false;
  uint32_t remainingMs_ = 0;
  uint32_t totalMs_ = 0;
  uint32_t lastUpdate_ = 0;
};
