#pragma once
#include <Arduino.h>

class Buzzer {
 public:
  struct Note { uint16_t frequency; uint16_t durationMs; };

  explicit Buzzer(uint8_t pin) : pin_(pin) {}
  void begin();
  void setEnabled(bool enabled);
  void playPetChirp(uint8_t id = 0);
  void playBoop();
  void playUiTick();
  void playTimerStart();
  void playDoneMelody(uint8_t id = 0);
  void playBreakMelody(uint8_t id = 0);
  void previewPetChirp(uint8_t id = 0);
  void previewDoneMelody(uint8_t id = 0);
  void previewBreakMelody(uint8_t id = 0);
  void previewTimerStart();
  void update(uint32_t now);
  void stop();
  bool isPlaying() const { return playing_; }

 private:
  void startSequence(const Note* notes, uint8_t count, bool force = false);

  uint8_t pin_;
  bool enabled_ = true;
  bool playing_ = false;
  bool noteActive_ = false;
  const Note* notes_ = nullptr;
  uint8_t count_ = 0;
  uint8_t index_ = 0;
  uint32_t noteStarted_ = 0;
};
