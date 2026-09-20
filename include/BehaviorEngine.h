#pragma once
#include <Arduino.h>
#include "PetVisual.h"

enum class BehaviorPriority : uint8_t {
  Ambient = 20,
  Gravity = 35,
  Physical = 55,
  User = 80,
  Critical = 100
};

struct BehaviorEvent {
  PetMood mood = PetMood::Idle;
  uint32_t startedAt = 0;
  uint16_t durationMs = 0;
  BehaviorPriority priority = BehaviorPriority::Ambient;
  bool explicitInteraction = false;
  bool valid = false;
};

class BehaviorEngine {
 public:
  void update(uint32_t now);
  bool request(PetMood mood, uint32_t now, uint16_t durationMs,
               BehaviorPriority priority, bool explicitInteraction = false,
               bool queueIfBlocked = true);
  void clear();
  bool active(uint32_t now) const;
  PetMood mood() const { return current_.mood; }
  BehaviorPriority priority() const { return current_.priority; }
  bool explicitInteraction() const { return current_.explicitInteraction; }
  float progress(uint32_t now) const;
  float envelope(uint32_t now) const;

 private:
  static constexpr uint8_t QUEUE_SIZE = 4;
  BehaviorEvent current_;
  BehaviorEvent queue_[QUEUE_SIZE];

  void start(const BehaviorEvent& event);
  bool enqueue(const BehaviorEvent& event);
  bool popNext(BehaviorEvent& event);
};
