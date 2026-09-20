#include "BehaviorEngine.h"

namespace {
float clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}
}

void BehaviorEngine::start(const BehaviorEvent& event) {
  current_ = event;
  current_.valid = true;
}

bool BehaviorEngine::active(uint32_t now) const {
  return current_.valid && static_cast<int32_t>(now - (current_.startedAt + current_.durationMs)) < 0;
}

float BehaviorEngine::progress(uint32_t now) const {
  if (!current_.valid || current_.durationMs == 0) return 1.0f;
  return clamp01(static_cast<float>(now - current_.startedAt) / static_cast<float>(current_.durationMs));
}

float BehaviorEngine::envelope(uint32_t now) const {
  const float p = progress(now);
  const float attack = clamp01(p / 0.14f);
  const float release = p > 0.78f ? clamp01((1.0f - p) / 0.22f) : 1.0f;
  const float a = attack * attack * (3.0f - 2.0f * attack);
  const float r = release * release * (3.0f - 2.0f * release);
  return a < r ? a : r;
}

bool BehaviorEngine::enqueue(const BehaviorEvent& event) {
  for (uint8_t i = 0; i < QUEUE_SIZE; ++i) {
    if (!queue_[i].valid) {
      queue_[i] = event;
      queue_[i].valid = true;
      return true;
    }
  }
  uint8_t weakest = 0;
  for (uint8_t i = 1; i < QUEUE_SIZE; ++i) {
    if (static_cast<uint8_t>(queue_[i].priority) < static_cast<uint8_t>(queue_[weakest].priority)) weakest = i;
  }
  if (static_cast<uint8_t>(event.priority) <= static_cast<uint8_t>(queue_[weakest].priority)) return false;
  queue_[weakest] = event;
  queue_[weakest].valid = true;
  return true;
}

bool BehaviorEngine::popNext(BehaviorEvent& event) {
  int8_t best = -1;
  for (uint8_t i = 0; i < QUEUE_SIZE; ++i) {
    if (!queue_[i].valid) continue;
    if (best < 0 || static_cast<uint8_t>(queue_[i].priority) > static_cast<uint8_t>(queue_[best].priority)) best = i;
  }
  if (best < 0) return false;
  event = queue_[best];
  queue_[best].valid = false;
  return true;
}

bool BehaviorEngine::request(PetMood mood, uint32_t now, uint16_t durationMs,
                             BehaviorPriority priority, bool explicitInteraction,
                             bool queueIfBlocked) {
  if (static_cast<uint8_t>(priority) >= static_cast<uint8_t>(BehaviorPriority::User)) {
    for (auto& queued : queue_) {
      if (queued.valid && static_cast<uint8_t>(queued.priority) < static_cast<uint8_t>(priority)) queued.valid = false;
    }
  }
  BehaviorEvent event;
  event.mood = mood;
  event.startedAt = now;
  event.durationMs = durationMs;
  event.priority = priority;
  event.explicitInteraction = explicitInteraction;
  event.valid = true;
  if (!active(now) || static_cast<uint8_t>(priority) >= static_cast<uint8_t>(current_.priority)) {
    start(event);
    return true;
  }
  return queueIfBlocked && enqueue(event);
}

void BehaviorEngine::update(uint32_t now) {
  if (active(now)) return;
  current_.valid = false;
  BehaviorEvent next;
  if (popNext(next)) {
    next.startedAt = now;
    start(next);
  }
}

void BehaviorEngine::clear() {
  current_ = BehaviorEvent{};
  for (auto& event : queue_) event = BehaviorEvent{};
}
