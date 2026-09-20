#include "Buzzer.h"

namespace {
const Buzzer::Note PET_0[] = {{988, 65}, {0, 25}, {1319, 90}};
const Buzzer::Note PET_1[] = {{1175, 55}, {1568, 70}};
const Buzzer::Note PET_2[] = {{659, 60}, {784, 60}, {988, 85}};
const Buzzer::Note BOOP[] = {{1760, 45}, {0, 20}, {2093, 70}};
const Buzzer::Note UI_TICK[] = {{1568, 22}};
const Buzzer::Note TIMER_START[] = {{784, 48}, {0, 18}, {1047, 55}, {0, 18}, {1568, 95}};

const Buzzer::Note DONE_0[] = {
  {784, 105}, {988, 105}, {1175, 125}, {0, 45},
  {988, 90}, {1175, 90}, {1568, 220}, {0, 60},
  {1319, 95}, {1568, 95}, {1976, 280}
};
const Buzzer::Note DONE_1[] = {
  {523, 90}, {659, 90}, {784, 90}, {1047, 180}, {0, 60}, {1319, 250}
};
const Buzzer::Note DONE_2[] = {
  {988, 70}, {1175, 70}, {1319, 70}, {1568, 70}, {1976, 230}
};
const Buzzer::Note DONE_3[] = {
  {659, 100}, {0, 40}, {659, 100}, {0, 40}, {988, 120}, {1319, 260}
};

const Buzzer::Note BREAK_0[] = {{659, 110}, {523, 110}, {392, 210}};
const Buzzer::Note BREAK_1[] = {{784, 90}, {659, 90}, {523, 160}, {392, 220}};
const Buzzer::Note BREAK_2[] = {{523, 80}, {0, 30}, {523, 80}, {0, 30}, {392, 220}};
}

template <size_t N>
static uint8_t noteCount(const Buzzer::Note (&)[N]) { return static_cast<uint8_t>(N); }

void Buzzer::begin() {
  pinMode(pin_, OUTPUT);
  noTone(pin_);
}

void Buzzer::setEnabled(bool enabled) {
  enabled_ = enabled;
  if (!enabled_) stop();
}

void Buzzer::startSequence(const Note* notes, uint8_t count, bool force) {
  if (!enabled_ && !force) return;
  stop();
  notes_ = notes;
  count_ = count;
  index_ = 0;
  playing_ = true;
  noteActive_ = false;
}

void Buzzer::playPetChirp(uint8_t id) {
  switch (id % 3) {
    case 1: startSequence(PET_1, noteCount(PET_1)); break;
    case 2: startSequence(PET_2, noteCount(PET_2)); break;
    default: startSequence(PET_0, noteCount(PET_0)); break;
  }
}

void Buzzer::playBoop() { startSequence(BOOP, noteCount(BOOP)); }
void Buzzer::playUiTick() { startSequence(UI_TICK, noteCount(UI_TICK)); }
void Buzzer::playTimerStart() { startSequence(TIMER_START, noteCount(TIMER_START)); }

void Buzzer::playDoneMelody(uint8_t id) {
  switch (id % 4) {
    case 1: startSequence(DONE_1, noteCount(DONE_1)); break;
    case 2: startSequence(DONE_2, noteCount(DONE_2)); break;
    case 3: startSequence(DONE_3, noteCount(DONE_3)); break;
    default: startSequence(DONE_0, noteCount(DONE_0)); break;
  }
}

void Buzzer::playBreakMelody(uint8_t id) {
  switch (id % 3) {
    case 1: startSequence(BREAK_1, noteCount(BREAK_1)); break;
    case 2: startSequence(BREAK_2, noteCount(BREAK_2)); break;
    default: startSequence(BREAK_0, noteCount(BREAK_0)); break;
  }
}

void Buzzer::previewPetChirp(uint8_t id) {
  switch (id % 3) {
    case 1: startSequence(PET_1, noteCount(PET_1), true); break;
    case 2: startSequence(PET_2, noteCount(PET_2), true); break;
    default: startSequence(PET_0, noteCount(PET_0), true); break;
  }
}

void Buzzer::previewDoneMelody(uint8_t id) {
  switch (id % 4) {
    case 1: startSequence(DONE_1, noteCount(DONE_1), true); break;
    case 2: startSequence(DONE_2, noteCount(DONE_2), true); break;
    case 3: startSequence(DONE_3, noteCount(DONE_3), true); break;
    default: startSequence(DONE_0, noteCount(DONE_0), true); break;
  }
}

void Buzzer::previewBreakMelody(uint8_t id) {
  switch (id % 3) {
    case 1: startSequence(BREAK_1, noteCount(BREAK_1), true); break;
    case 2: startSequence(BREAK_2, noteCount(BREAK_2), true); break;
    default: startSequence(BREAK_0, noteCount(BREAK_0), true); break;
  }
}

void Buzzer::previewTimerStart() { startSequence(TIMER_START, noteCount(TIMER_START), true); }

void Buzzer::update(uint32_t now) {
  if (!playing_) return;

  if (!noteActive_) {
    if (index_ >= count_) {
      stop();
      return;
    }
    const Note& n = notes_[index_];
    if (n.frequency > 0) tone(pin_, n.frequency);
    else noTone(pin_);
    noteStarted_ = now;
    noteActive_ = true;
    return;
  }

  if ((now - noteStarted_) >= notes_[index_].durationMs) {
    noTone(pin_);
    index_++;
    noteActive_ = false;
  }
}

void Buzzer::stop() {
  noTone(pin_);
  playing_ = false;
  noteActive_ = false;
  notes_ = nullptr;
  count_ = 0;
  index_ = 0;
}
