#include <Arduino.h>
#include <math.h>
#include <cstring>
#include "ProjectConfig.h"
#include "Settings.h"
#include "SettingsStore.h"
#include "PetState.h"
#include "PetStateStore.h"
#include "ButtonInput.h"
#include "SideTouchInput.h"
#include "SensorHub.h"
#include "Buzzer.h"
#include "PomodoroTimer.h"
#include "DisplayUI.h"
#include "BleConfigService.h"
#include "BehaviorEngine.h"

enum class AppMode : uint8_t { Pet, PomodoroReady, Pomodoro, Celebration, PetMenu };
enum class TimerMode : uint8_t { Focus = 0, ShortBreak = 1, LongBreak = 2 };
enum class Interaction : uint8_t { None, Pet, Boop };
enum class SpeechContext : uint8_t {
  None, Boot, Pet, Pickup, Settled, Shake, FocusDone, BreakDone, Sleep, Wake
};

Settings settings;
SettingsStore settingsStore;
PetState petState;
PetStateStore petStateStore;
ButtonInput button(hw::BUTTON_PIN);
SideTouchInput touchLeft(hw::TOUCH_LEFT_PIN, hw::TOUCH_ACTIVE_HIGH);
SideTouchInput touchRight(hw::TOUCH_RIGHT_PIN, hw::TOUCH_ACTIVE_HIGH);
SensorHub sensorHub;
Buzzer buzzer(hw::BUZZER_PIN);
PomodoroTimer pomodoro;
DisplayUI ui;
BleConfigService bleConfig;

AppMode mode = AppMode::Pet;
PetMood mood = PetMood::Idle;
Interaction interaction = Interaction::None;
uint32_t nextFaceAt = 0;
uint32_t petEffectStarted = 0;
bool petEffectActive = false;
uint32_t celebrationStarted = 0;
uint32_t lastFrameAt = 0;
uint32_t lastNeedsAt = 0;
uint32_t needsMinuteCounter = 0;
bool celebrationWasBreak = false;
TimerMode selectedTimerMode = TimerMode::Focus;
TimerMode previousTimerMode = TimerMode::Focus;
int8_t timerModeDirection = 1;
uint32_t timerModeTransitionStarted = 0;
constexpr uint16_t TIMER_MODE_TRANSITION_MS = 220;

bool clockValid = false;
uint32_t syncEpoch = 0;
uint32_t syncAtMs = 0;
int16_t tzOffsetMinutes = 0;
uint32_t sleepOverrideUntil = 0;
uint32_t suppressInputUntil = 0;
bool petStateDirty = false;
BehaviorEngine behavior;

bool speechActive = false;
uint32_t speechStarted = 0;
uint16_t speechDuration = 5400;
char speechText[20] = {};
SpeechContext pendingSpeechContext = SpeechContext::None;
uint32_t pendingSpeechAt = 0;
bool pendingSpeechForce = false;
uint32_t speechCooldownUntil = 0;
uint32_t bootStartedAt = 0;
bool bootGreetingDone = false;
bool previousSleepState = false;
uint32_t nextRareEventAt = 0;
uint32_t lastLeftTouchAt = 0;
uint32_t lastRightTouchAt = 0;
bool leftHoldActionFired = false;
bool rightHoldActionFired = false;
bool hugActionFired = false;
uint32_t touchGestureCooldownUntil = 0;
uint32_t lastSensorTelemetryAt = 0;
uint32_t lastSensorUpdateAt = 0;
uint32_t petStateSaveAfter = 0;
uint32_t lastButtonActivityAt = 0;
uint32_t lastCompanionActivityAt = 0;
bool oledDimmed = false;
uint8_t previousPersonality = 0;
uint32_t skinChangeStarted = 0;
uint32_t diagnosticWindowAt = 0;
uint32_t loopCounter = 0;
uint32_t renderedFrameCounter = 0;
float measuredLoopHz = 0.0f;
float measuredRenderFps = 0.0f;

enum class DeskOrientation : uint8_t { Center, Left, Right, Up, Down, FaceDown };
DeskOrientation lastDeskOrientation = DeskOrientation::Center;
DeskOrientation orientationCandidate = DeskOrientation::Center;
uint32_t orientationCandidateSince = 0;
bool strongShakeAwaitingSettle = false;
uint32_t strongShakeAt = 0;

PetMenuView menuView = PetMenuView::Root;
uint8_t menuIndex = 0;
uint8_t menuPreviousIndex = 0;
int8_t menuDirection = 1;
uint32_t menuTransitionStarted = 0;
constexpr uint16_t MENU_TRANSITION_MS = 180;
Settings menuSettingsSnapshot;
bool menuSettingsSnapshotValid = false;

static uint8_t addClamped(uint8_t value, int16_t delta) {
  int16_t out = static_cast<int16_t>(value) + delta;
  return static_cast<uint8_t>(constrain(out, 0, 100));
}

void addXp(uint32_t amount) {
  petState.xp += amount;
  PetStateStore::sanitize(petState);
}

void markPetStateDirty(uint32_t now, uint16_t delayMs = 2200) {
  petStateDirty = true;
  petStateSaveAfter = now + delayMs;
}

void flushPetStateIfDue(uint32_t now) {
  if (!petStateDirty) return;
  if (static_cast<int32_t>(now - petStateSaveAfter) < 0) return;
  if (button.isPressed()) return;
  petStateStore.save(petState);
  petStateDirty = false;
}

uint8_t effectiveIdleSpeed() {
  uint16_t speed = settings.idleAnimationSpeed;
  if (settings.skinPersonalityEnabled) {
    speed = static_cast<uint16_t>((speed * petBehaviorProfile(settings.personality).idleSpeedPercent) / 100U);
  }
  return static_cast<uint8_t>(constrain(speed, 50U, 180U));
}

uint32_t randomFaceDelayMs() {
  const uint32_t minMs = static_cast<uint32_t>(settings.faceMinSeconds) * 1000UL;
  const uint32_t maxMs = static_cast<uint32_t>(settings.faceMaxSeconds) * 1000UL;
  uint32_t delayMs = maxMs <= minMs ? minMs : random(minMs, maxMs + 1);
  if (settings.skinPersonalityEnabled) {
    const uint16_t speed = petBehaviorProfile(settings.personality).idleSpeedPercent;
    delayMs = static_cast<uint32_t>((static_cast<uint64_t>(delayMs) * 100ULL) / max<uint16_t>(50, speed));
  }
  return delayMs;
}

int64_t localEpoch(uint32_t now) {
  if (!clockValid) return -1;
  return static_cast<int64_t>(syncEpoch) + static_cast<int64_t>((now - syncAtMs) / 1000UL) + static_cast<int64_t>(tzOffsetMinutes) * 60LL;
}

int32_t localDay(uint32_t now) {
  const int64_t epoch = localEpoch(now);
  return epoch < 0 ? -1 : static_cast<int32_t>(epoch / 86400LL);
}

uint8_t localHour(uint32_t now) {
  const int64_t epoch = localEpoch(now);
  if (epoch < 0) return 255;
  return static_cast<uint8_t>((epoch % 86400LL) / 3600LL);
}

bool isSleepWindow(uint32_t now) {
  if (!settings.sleepEnabled || !clockValid) return false;
  if (static_cast<int32_t>(now - sleepOverrideUntil) < 0) return false;
  const uint8_t hour = localHour(now);
  if (hour > 23) return false;
  if (settings.sleepStartHour == settings.sleepEndHour) return true;
  if (settings.sleepStartHour < settings.sleepEndHour) {
    return hour >= settings.sleepStartHour && hour < settings.sleepEndHour;
  }
  return hour >= settings.sleepStartHour || hour < settings.sleepEndHour;
}

void updateToday(uint32_t now) {
  const int32_t day = localDay(now);
  if (day < 0) return;
  if (petState.todayDay != day) {
    petState.todayDay = day;
    petState.todaySessions = 0;
    petState.todayMinutes = 0;
    petStateStore.save(petState);
  }
}

void updateStreak(uint32_t now) {
  const int32_t day = localDay(now);
  if (day < 0) return;

  if (petState.lastFocusDay == day) return;
  if (petState.lastFocusDay >= 0 && day == petState.lastFocusDay + 1) {
    petState.streakDays++;
  } else {
    petState.streakDays = 1;
  }
  petState.lastFocusDay = day;
}


const char* chooseSpeech(SpeechContext context) {
  static const char* const BOOT_NEUTRAL[] = {"Hello!", "Good Again!", "Hi There", "Oh Hi", "Good Day"};
  static const char* const PET_NEUTRAL[] = {"Happy Happy", "So Happy", "Love That", "Thank You", "Best Pats"};
  static const char* const BOOT_CUTE[] = {"Hiii!", "Yay Hello", "Tiny Hello", "Good Morning"};
  static const char* const PET_CUTE[] = {"Hehe!", "More Love", "So Cozy", "Heart Eyes"};
  static const char* const BOOT_ROBOT[] = {"BOOT OK", "HELLO USER", "SYSTEM READY", "ONLINE"};
  static const char* const PET_ROBOT[] = {"INPUT NICE", "JOY PLUS", "PAT OK", "BOND UP"};
  static const char* const BOOT_MIN[] = {"Hello", "Ready"};
  static const char* const PET_MIN[] = {"Happy", "Nice"};

  static const char* const PICKUP[] = {"Whoa!", "Going Up", "Oh Hey", "Adventure?"};
  static const char* const SETTLED[] = {"Nice Desk", "All Good", "Comfy Here", "Home Again"};
  static const char* const SHAKE[] = {"Whoa Whoa", "Dizzy Me", "Too Much", "Spinny!"};
  static const char* const FOCUS[] = {"Nice Work", "Focus Win", "We Did", "Great Job", "All Done"};
  static const char* const BREAK[] = {"Break Done", "Ready Again", "Back Soon", "Refreshed!"};
  static const char* const SLEEP[] = {"Good Night", "Nap Time", "Sleepy Time", "Night Night"};
  static const char* const WAKE[] = {"Morning!", "I'm Up", "Hi Again", "New Day"};

  if (settings.speechPack == 4) {
    if (context == SpeechContext::Boot) return settings.customBootText;
    if (context == SpeechContext::Pet) return settings.customPetText;
  }

  const char* const* lines = BOOT_NEUTRAL;
  size_t count = sizeof(BOOT_NEUTRAL) / sizeof(BOOT_NEUTRAL[0]);

  if (context == SpeechContext::Boot || context == SpeechContext::Pet) {
    const bool pet = context == SpeechContext::Pet;
    switch (settings.speechPack) {
      case 1:
        lines = pet ? PET_CUTE : BOOT_CUTE;
        count = pet ? sizeof(PET_CUTE) / sizeof(PET_CUTE[0]) : sizeof(BOOT_CUTE) / sizeof(BOOT_CUTE[0]);
        break;
      case 2:
        lines = pet ? PET_ROBOT : BOOT_ROBOT;
        count = pet ? sizeof(PET_ROBOT) / sizeof(PET_ROBOT[0]) : sizeof(BOOT_ROBOT) / sizeof(BOOT_ROBOT[0]);
        break;
      case 3:
        lines = pet ? PET_MIN : BOOT_MIN;
        count = pet ? sizeof(PET_MIN) / sizeof(PET_MIN[0]) : sizeof(BOOT_MIN) / sizeof(BOOT_MIN[0]);
        break;
      default:
        lines = pet ? PET_NEUTRAL : BOOT_NEUTRAL;
        count = pet ? sizeof(PET_NEUTRAL) / sizeof(PET_NEUTRAL[0]) : sizeof(BOOT_NEUTRAL) / sizeof(BOOT_NEUTRAL[0]);
        break;
    }
    return lines[random(0, static_cast<long>(count))];
  }

  switch (context) {
    case SpeechContext::Pickup:    lines = PICKUP; count = sizeof(PICKUP) / sizeof(PICKUP[0]); break;
    case SpeechContext::Settled:   lines = SETTLED; count = sizeof(SETTLED) / sizeof(SETTLED[0]); break;
    case SpeechContext::Shake:     lines = SHAKE; count = sizeof(SHAKE) / sizeof(SHAKE[0]); break;
    case SpeechContext::FocusDone: lines = FOCUS; count = sizeof(FOCUS) / sizeof(FOCUS[0]); break;
    case SpeechContext::BreakDone: lines = BREAK; count = sizeof(BREAK) / sizeof(BREAK[0]); break;
    case SpeechContext::Sleep:     lines = SLEEP; count = sizeof(SLEEP) / sizeof(SLEEP[0]); break;
    case SpeechContext::Wake:      lines = WAKE; count = sizeof(WAKE) / sizeof(WAKE[0]); break;
    default: break;
  }
  return lines[random(0, static_cast<long>(count))];
}

void queueSpeech(SpeechContext context, uint32_t at, bool force = false) {
  if (!settings.speechBubblesEnabled || context == SpeechContext::None) return;
  if (pendingSpeechContext != SpeechContext::None && pendingSpeechForce && !force) return;
  pendingSpeechContext = context;
  pendingSpeechAt = at;
  pendingSpeechForce = force;
}

bool canStartSpeech(uint32_t now) {
  if (!settings.speechBubblesEnabled || mode != AppMode::Pet || speechActive) return false;
  if (petEffectActive || button.isPressed()) return false;
  if (static_cast<int32_t>(now - speechCooldownUntil) < 0) return false;
  return true;
}

void updateSpeech(uint32_t now) {
  if (speechActive && (now - speechStarted) >= speechDuration) {
    speechActive = false;
    speechText[0] = '\0';
    speechCooldownUntil = now + 1200UL;
  }

  if (pendingSpeechContext == SpeechContext::None ||
      static_cast<int32_t>(now - pendingSpeechAt) < 0 ||
      !canStartSpeech(now)) {
    return;
  }

  const bool allowed = pendingSpeechForce || random(0, 100) < settings.speechEventChance;
  const SpeechContext context = pendingSpeechContext;
  pendingSpeechContext = SpeechContext::None;
  pendingSpeechForce = false;
  if (!allowed) return;

  const char* text = chooseSpeech(context);
  strncpy(speechText, text, sizeof(speechText) - 1);
  speechText[sizeof(speechText) - 1] = '\0';
  speechStarted = now;
  // Typing is kept brisk in DisplayUI; most of this time is an intentional
  // reading pause so the bubble does not disappear as soon as it is understood.
  const size_t speechLen = strlen(speechText);
  speechDuration = static_cast<uint16_t>(5200U + min<size_t>(speechLen, 12U) * 55U);
  speechActive = true;
}

PetMood randomMood() {
  const SensorSnapshot& sensors = sensorHub.snapshot();
  if (settings.deskBuddyEnabled && settings.environmentReactionsEnabled && sensors.bmpAvailable && random(0, 3) == 0) {
    if (!isnan(sensors.temperatureC) && sensors.temperatureC >= 30.0f) return PetMood::Sleepy;
    if (!isnan(sensors.temperatureC) && sensors.temperatureC <= 15.0f) return PetMood::Surprised;
  }
  if (petState.energy < 24) return (random(0, 3) == 0) ? PetMood::Blink : PetMood::Sleepy;
  if (petState.mood < 28) return (random(0, 2) == 0) ? PetMood::Sleepy : PetMood::Blink;
  if (!settings.skinPersonalityEnabled) {
    switch (random(0, 6)) {
      case 0: return PetMood::Blink;
      case 1: return PetMood::Curious;
      case 2: return PetMood::Focused;
      case 3: return PetMood::Idle;
      case 4: return PetMood::Happy;
      default: return petState.energy < 45 ? PetMood::Sleepy : PetMood::Idle;
    }
  }

  const PetBehaviorProfile& profile = petBehaviorProfile(settings.personality);
  const uint16_t total = profile.blinkWeight + profile.curiousWeight + profile.happyWeight +
                         profile.focusedWeight + profile.sleepyWeight + profile.excitedWeight + 24;
  uint16_t roll = static_cast<uint16_t>(random(0, total));
  if (roll < profile.blinkWeight) return PetMood::Blink;
  roll -= profile.blinkWeight;
  if (roll < profile.curiousWeight) return PetMood::Curious;
  roll -= profile.curiousWeight;
  if (roll < profile.happyWeight) return PetMood::Happy;
  roll -= profile.happyWeight;
  if (roll < profile.focusedWeight) return PetMood::Focused;
  roll -= profile.focusedWeight;
  if (roll < profile.sleepyWeight) return PetMood::Sleepy;
  roll -= profile.sleepyWeight;
  if (roll < profile.excitedWeight) return PetMood::Excited;
  return PetMood::Idle;
}

void scheduleNextMood(uint32_t now) {
  nextFaceAt = now + randomFaceDelayMs();
}

void scheduleRareEvent(uint32_t now) {
  const uint32_t minMs = static_cast<uint32_t>(settings.rareEventMinSeconds) * 1000UL;
  const uint32_t maxMs = static_cast<uint32_t>(settings.rareEventMaxSeconds) * 1000UL;
  nextRareEventAt = now + (maxMs <= minMs ? minMs : random(minMs, maxMs + 1UL));
}

uint8_t highestUnlockedAccessory() {
  if (petState.level >= 7) return 3;
  if (petState.level >= 4) return 2;
  if (petState.level >= 2) return 1;
  return 0;
}

void returnToPet(uint32_t now, PetMood returnMood);

uint8_t activeAccessory() {
  const uint8_t unlocked = highestUnlockedAccessory();
  if (settings.accessoryMode == 255) return unlocked;
  return (settings.accessoryMode < unlocked) ? settings.accessoryMode : unlocked;
}

uint8_t menuCount(PetMenuView view) {
  switch (view) {
    case PetMenuView::Root: return 5;
    case PetMenuView::Stats: return 4;          // 3 pages + SAVE & BACK
    case PetMenuView::Accessory: return 6;      // 5 choices + SAVE & BACK
    case PetMenuView::Personality: return PET_SKIN_COUNT + 1; // skins + SAVE & BACK
    case PetMenuView::Options: return 10;       // 9 sections + SAVE & BACK
    case PetMenuView::DeskBuddy: return 3;      // off/on + SAVE & BACK
    case PetMenuView::Sound: return 3;          // 2 choices + SAVE & BACK
    case PetMenuView::Sleep: return 3;          // 2 choices + SAVE & BACK
    case PetMenuView::PetSound: return 4;       // 3 choices + SAVE & BACK
    case PetMenuView::DoneMelody: return 5;     // 4 choices + SAVE & BACK
    case PetMenuView::BreakMelody: return 4;    // 3 choices + SAVE & BACK
    case PetMenuView::ScreenRotation: return 3; // normal/180 + SAVE & BACK
    case PetMenuView::TouchLayout: return 3;     // normal/swapped + SAVE & BACK
    case PetMenuView::MpuCalibration: return 3;  // calibrate/reset + SAVE & BACK
  }
  return 1;
}

void setMenuSelection(uint8_t next, int8_t direction, uint32_t now) {
  const uint8_t count = menuCount(menuView);
  if (count == 0) return;
  menuPreviousIndex = menuIndex;
  menuIndex = next % count;
  menuDirection = direction >= 0 ? 1 : -1;
  menuTransitionStarted = now;
}

void moveMenu(int8_t delta, uint32_t now) {
  const uint8_t count = menuCount(menuView);
  int16_t next = static_cast<int16_t>(menuIndex) + delta;
  while (next < 0) next += count;
  while (next >= count) next -= count;
  setMenuSelection(static_cast<uint8_t>(next), delta, now);
  buzzer.playUiTick();
}

void enterMenuView(PetMenuView view, uint8_t initialIndex, uint32_t now) {
  menuView = view;
  menuIndex = initialIndex % menuCount(view);
  menuPreviousIndex = menuIndex;
  menuDirection = 1;
  menuTransitionStarted = now - MENU_TRANSITION_MS;
  if (view != PetMenuView::Root) {
    menuSettingsSnapshot = settings;
    menuSettingsSnapshotValid = true;
  } else {
    menuSettingsSnapshotValid = false;
  }
}

void openPetMenu(uint32_t now) {
  behavior.clear();
  buzzer.stop();
  interaction = Interaction::None;
  petEffectActive = false;
  mode = AppMode::PetMenu;
  enterMenuView(PetMenuView::Root, 0, now);
}

void returnMenuRoot(uint8_t rootIndex, uint32_t now) {
  enterMenuView(PetMenuView::Root, rootIndex, now);
}

void returnOptions(uint8_t optionIndex, uint32_t now) {
  enterMenuView(PetMenuView::Options, optionIndex, now);
}

void applyRuntimeSettings() {
  ui.setContrast(oledDimmed ? 1 : settings.oledContrast);
  ui.setFlipped(settings.screenFlipped);
  buzzer.setEnabled(settings.soundEnabled);
  sensorHub.setMountRotation(settings.mpuMountRotation);
  sensorHub.setGazeCalibration(settings.mpuGazeOffsetX, settings.mpuGazeOffsetY);
  ui.setAnimationTuning(effectiveIdleSpeed(), settings.heartParticleCount, settings.skinPersonalityEnabled);
}

void saveMenuSettings() {
  settingsStore.save(settings);
  applyRuntimeSettings();
}

void backFromMenu(uint32_t now) {
  buzzer.playUiTick();
  if (menuView == PetMenuView::Root) {
    returnToPet(now, PetMood::Idle);
    return;
  }

  // Long-press is a real Back action: discard un-saved edits made in this view.
  if (menuSettingsSnapshotValid) {
    settings = menuSettingsSnapshot;
    applyRuntimeSettings();
  }

  switch (menuView) {
    case PetMenuView::DeskBuddy: returnOptions(0, now); break;
    case PetMenuView::Sound: returnOptions(1, now); break;
    case PetMenuView::Sleep: returnOptions(2, now); break;
    case PetMenuView::ScreenRotation: returnOptions(3, now); break;
    case PetMenuView::TouchLayout: returnOptions(4, now); break;
    case PetMenuView::MpuCalibration: returnOptions(5, now); break;
    case PetMenuView::PetSound: returnOptions(6, now); break;
    case PetMenuView::DoneMelody: returnOptions(7, now); break;
    case PetMenuView::BreakMelody: returnOptions(8, now); break;
    case PetMenuView::Stats: returnMenuRoot(0, now); break;
    case PetMenuView::Accessory: returnMenuRoot(1, now); break;
    case PetMenuView::Personality: returnMenuRoot(2, now); break;
    case PetMenuView::Options: returnMenuRoot(3, now); break;
    default: returnToPet(now, PetMood::Idle); break;
  }
}

void validateMenu(uint32_t now) {
  buzzer.playUiTick();
  if (menuView == PetMenuView::Root) {
    switch (menuIndex) {
      case 0: enterMenuView(PetMenuView::Stats, 0, now); break;
      case 1: {
        const uint8_t selected = settings.accessoryMode == 255 ? 0 : static_cast<uint8_t>(settings.accessoryMode + 1);
        enterMenuView(PetMenuView::Accessory, selected, now);
        break;
      }
      case 2: enterMenuView(PetMenuView::Personality, settings.personality, now); break;
      case 3: enterMenuView(PetMenuView::Options, 0, now); break;
      default: returnToPet(now, PetMood::Idle); break;
    }
    return;
  }

  // Every submenu ends with the same explicit SAVE & BACK card.
  // Choices are applied in RAM while browsing; persistence happens here.
  const uint8_t saveBackIndex = menuCount(menuView) - 1;
  if (menuIndex == saveBackIndex) {
    saveMenuSettings();
    switch (menuView) {
      case PetMenuView::DeskBuddy: returnOptions(0, now); break;
      case PetMenuView::Sound: returnOptions(1, now); break;
      case PetMenuView::Sleep: returnOptions(2, now); break;
      case PetMenuView::ScreenRotation: returnOptions(3, now); break;
      case PetMenuView::TouchLayout: returnOptions(4, now); break;
      case PetMenuView::MpuCalibration: returnOptions(5, now); break;
      case PetMenuView::PetSound: returnOptions(6, now); break;
      case PetMenuView::DoneMelody: returnOptions(7, now); break;
      case PetMenuView::BreakMelody: returnOptions(8, now); break;
      case PetMenuView::Stats: returnMenuRoot(0, now); break;
      case PetMenuView::Accessory: returnMenuRoot(1, now); break;
      case PetMenuView::Personality: returnMenuRoot(2, now); break;
      case PetMenuView::Options: returnMenuRoot(3, now); break;
      default: break;
    }
    return;
  }

  if (menuView == PetMenuView::Options) {
    switch (menuIndex) {
      case 0: enterMenuView(PetMenuView::DeskBuddy, settings.deskBuddyEnabled ? 1 : 0, now); break;
      case 1: enterMenuView(PetMenuView::Sound, settings.soundEnabled ? 1 : 0, now); break;
      case 2: enterMenuView(PetMenuView::Sleep, settings.sleepEnabled ? 1 : 0, now); break;
      case 3: enterMenuView(PetMenuView::ScreenRotation, settings.screenFlipped ? 1 : 0, now); break;
      case 4: enterMenuView(PetMenuView::TouchLayout, settings.swapTouchButtons ? 1 : 0, now); break;
      case 5: enterMenuView(PetMenuView::MpuCalibration, 0, now); break;
      case 6: enterMenuView(PetMenuView::PetSound, settings.petSound, now); break;
      case 7: enterMenuView(PetMenuView::DoneMelody, settings.doneMelody, now); break;
      case 8: enterMenuView(PetMenuView::BreakMelody, settings.breakMelody, now); break;
    }
    return;
  }

  // Stats pages are informational. Use SAVE & BACK to leave the submenu.
  if (menuView == PetMenuView::Stats) return;

  if (menuView == PetMenuView::Accessory) {
    if (menuIndex == 0) settings.accessoryMode = 255;
    else if (menuIndex == 1) settings.accessoryMode = 0;
    else {
      const uint8_t wanted = menuIndex - 1;
      if (wanted > highestUnlockedAccessory()) return;
      settings.accessoryMode = wanted;
    }
    return;
  }

  if (menuView == PetMenuView::Personality) {
    settings.personality = menuIndex;
    return;
  }

  if (menuView == PetMenuView::DeskBuddy) {
    settings.deskBuddyEnabled = menuIndex == 1;
    return;
  }

  if (menuView == PetMenuView::Sound) {
    settings.soundEnabled = menuIndex == 1;
    buzzer.setEnabled(settings.soundEnabled);
    return;
  }

  if (menuView == PetMenuView::Sleep) {
    settings.sleepEnabled = menuIndex == 1;
    return;
  }

  if (menuView == PetMenuView::ScreenRotation) {
    settings.screenFlipped = menuIndex == 1;
    ui.setFlipped(settings.screenFlipped);
    return;
  }

  if (menuView == PetMenuView::TouchLayout) {
    settings.swapTouchButtons = menuIndex == 1;
    return;
  }

  if (menuView == PetMenuView::MpuCalibration) {
    if (menuIndex == 0) {
      float offsetX = 0.0f, offsetY = 0.0f;
      if (sensorHub.calibrateCurrent(offsetX, offsetY)) {
        settings.mpuGazeOffsetX = offsetX;
        settings.mpuGazeOffsetY = offsetY;
      }
    } else if (menuIndex == 1) {
      settings.mpuGazeOffsetX = 0.0f;
      settings.mpuGazeOffsetY = 0.0f;
      sensorHub.resetCalibration();
    }
    return;
  }

  if (menuView == PetMenuView::PetSound) {
    settings.petSound = menuIndex;
    return;
  }

  if (menuView == PetMenuView::DoneMelody) {
    settings.doneMelody = menuIndex;
    return;
  }

  if (menuView == PetMenuView::BreakMelody) {
    settings.breakMelody = menuIndex;
  }
}

void returnToPet(uint32_t now, PetMood returnMood = PetMood::Happy) {
  behavior.clear();
  mode = AppMode::Pet;
  mood = returnMood;
  interaction = Interaction::None;
  petEffectActive = true;
  petEffectStarted = now;
  scheduleNextMood(now);
}

uint16_t timerModeMinutes(TimerMode timerMode) {
  switch (timerMode) {
    case TimerMode::ShortBreak: return settings.breakMinutes;
    case TimerMode::LongBreak: return settings.longBreakMinutes;
    default: return settings.focusMinutes;
  }
}

bool timerModeIsBreak(TimerMode timerMode) {
  return timerMode != TimerMode::Focus;
}

void enterPomodoroReady(TimerMode timerMode, uint32_t now) {
  behavior.clear();
  buzzer.stop();
  pomodoro.cancel();
  selectedTimerMode = timerMode;
  previousTimerMode = timerMode;
  timerModeDirection = 1;
  timerModeTransitionStarted = now - TIMER_MODE_TRANSITION_MS;
  mode = AppMode::PomodoroReady;
  petEffectActive = false;
  interaction = Interaction::None;
}

void cycleTimerMode(int8_t delta, uint32_t now) {
  previousTimerMode = selectedTimerMode;
  int8_t next = static_cast<int8_t>(selectedTimerMode) + delta;
  while (next < 0) next += 3;
  while (next >= 3) next -= 3;
  selectedTimerMode = static_cast<TimerMode>(next);
  timerModeDirection = delta >= 0 ? 1 : -1;
  timerModeTransitionStarted = now;
  buzzer.playUiTick();
}

void startSelectedTimer(uint32_t now) {
  const uint16_t minutes = timerModeMinutes(selectedTimerMode);
  pomodoro.start(minutes, timerModeIsBreak(selectedTimerMode));
  buzzer.playTimerStart();
  mode = AppMode::Pomodoro;
  // The release that starts the timer would otherwise emit shortPress after
  // the double-click window and immediately pause the fresh timer.
  suppressInputUntil = now + settings.doubleClickMs + 100UL;
}

void beginCelebration(uint32_t now, bool wasBreak) {
  celebrationWasBreak = wasBreak;
  celebrationStarted = now;
  mode = AppMode::Celebration;
  if (wasBreak) buzzer.playBreakMelody(settings.breakMelody);
  else buzzer.playDoneMelody(settings.doneMelody);
}

void beginPetFeedback(uint32_t now) {
  sleepOverrideUntil = now + 60000UL;
  mood = PetMood::Affectionate;
  interaction = Interaction::Pet;
  petEffectActive = true;
  petEffectStarted = now;
  buzzer.playPetChirp(settings.petSound);
  scheduleNextMood(now);
}

void commitPetInteraction(uint32_t now) {
  petState.pets++;
  petState.mood = addClamped(petState.mood, 8);
  petState.affection = addClamped(petState.affection, 5);
  petState.energy = addClamped(petState.energy, 1);
  addXp(1);
  // Do not block the visible interaction with a LittleFS write.
  markPetStateDirty(now);
}

void applyBoop(uint32_t now) {
  petState.boops++;
  petState.mood = addClamped(petState.mood, 4);
  petState.affection = addClamped(petState.affection, 2);
  addXp(1);
  markPetStateDirty(now);

  sleepOverrideUntil = now + 60000UL;
  mood = PetMood::Surprised;
  interaction = Interaction::Boop;
  petEffectActive = true;
  petEffectStarted = now;
  buzzer.playBoop();
  scheduleNextMood(now);
}


void recordFocusHistory(uint32_t now, uint16_t minutes) {
  const int32_t day = localDay(now);
  if (day < 0) return;
  const uint8_t slot = static_cast<uint8_t>(day % 7);
  if (petState.historyDay[slot] != day) {
    petState.historyDay[slot] = day;
    petState.historySessions[slot] = 0;
    petState.historyMinutes[slot] = 0;
  }
  if (petState.historySessions[slot] < 65535) petState.historySessions[slot]++;
  const uint32_t nextMinutes = static_cast<uint32_t>(petState.historyMinutes[slot]) + minutes;
  petState.historyMinutes[slot] = nextMinutes > 65535UL ? 65535 : static_cast<uint16_t>(nextMinutes);
}

void completeFocus(uint32_t now) {
  petState.focusSessions++;
  petState.focusMinutes += settings.focusMinutes;
  petState.focusXp += static_cast<uint32_t>(settings.focusMinutes) * 2UL;
  petState.todaySessions++;
  petState.todayMinutes += settings.focusMinutes;
  petState.mood = addClamped(petState.mood, 10);
  petState.affection = addClamped(petState.affection, 3);
  petState.energy = addClamped(petState.energy, -7);
  addXp(static_cast<uint32_t>(settings.focusMinutes) * 2UL);
  updateStreak(now);
  recordFocusHistory(now, settings.focusMinutes);
  petStateStore.save(petState);
}

void completeBreak() {
  petState.energy = addClamped(petState.energy, 8);
  petState.mood = addClamped(petState.mood, 4);
  petStateStore.save(petState);
}

DeskOrientation classifyDeskOrientation(const SensorSnapshot& sensors) {
  if (!sensors.mpuAvailable) return DeskOrientation::Center;
  if (sensors.gravityZ < -0.45f) return DeskOrientation::FaceDown;
  if (fabsf(sensors.gazeX) > fabsf(sensors.gazeY) && fabsf(sensors.gazeX) > 0.56f) {
    return sensors.gazeX < 0.0f ? DeskOrientation::Left : DeskOrientation::Right;
  }
  if (fabsf(sensors.gazeY) > 0.58f) return sensors.gazeY < 0.0f ? DeskOrientation::Up : DeskOrientation::Down;
  return DeskOrientation::Center;
}

void triggerDeskMood(PetMood nextMood, uint32_t now, uint16_t durationMs,
                     bool explicitInteraction = false,
                     BehaviorPriority priority = BehaviorPriority::Ambient) {
  if (explicitInteraction && static_cast<uint8_t>(priority) < static_cast<uint8_t>(BehaviorPriority::User)) {
    priority = BehaviorPriority::User;
  }
  behavior.request(nextMood, now, durationMs, priority, explicitInteraction, !explicitInteraction);
  scheduleNextMood(now);
}

void triggerRareEvent(uint32_t now) {
  if (!settings.rareEventsEnabled || isSleepWindow(now)) return;
  PetMood eventMood = PetMood::Yawn;
  uint16_t duration = 1700;
  switch (random(0, 5)) {
    case 0: eventMood = PetMood::Yawn; duration = 2100; break;
    case 1: eventMood = PetMood::Sneeze; duration = 1300; break;
    case 2: eventMood = PetMood::Dance; duration = 2200; break;
    case 3: eventMood = PetMood::Dream; duration = 2500; break;
    default: eventMood = PetMood::Hiccup; duration = 1600; break;
  }
  triggerDeskMood(eventMood, now, duration);
  petState.rareEvents++;
  markPetStateDirty(now, 5000);
  scheduleRareEvent(now);
}

void applyTouchReward(PetMood touchMood, uint32_t now, uint16_t duration, int moodDelta, int affectionDelta) {
  triggerDeskMood(touchMood, now, duration, true);
  petState.mood = addClamped(petState.mood, moodDelta);
  petState.affection = addClamped(petState.affection, affectionDelta);
  addXp(1);
  markPetStateDirty(now);
}

void tickNeeds(uint32_t now) {
  if ((now - lastNeedsAt) < app::NEEDS_TICK_MS) return;
  lastNeedsAt = now;
  needsMinuteCounter++;

  // The companion never penalizes absence. Mood and affection do not decay just
  // because the user is working, and no hidden boredom meter grows over time.
  // During Pomodoro/menu flows even passive energy drift is frozen.
  if (mode != AppMode::Pet) return;

  bool changed = false;
  if (isSleepWindow(now)) {
    if ((needsMinuteCounter % 5) == 0 && petState.energy < 100) { petState.energy++; changed = true; }
  } else if ((needsMinuteCounter % 20) == 0 && petState.energy > 0) {
    petState.energy--;
    changed = true;
  }

  if (changed && (needsMinuteCounter % 10) == 0) petStateStore.save(petState);
}

void setup() {
  Serial.begin(app::SERIAL_BAUD);
  delay(80);

  randomSeed(micros() ^ analogRead(A0));
  settingsStore.begin();
  settingsStore.load(settings);
  petStateStore.load(petState);

  button.begin();
  touchLeft.begin();
  touchRight.begin();
  buzzer.begin();
  buzzer.setEnabled(settings.soundEnabled);

  if (!ui.begin(settings.oledContrast)) {
    Serial.println("OLED init failed. Check 0x3C / SH1106 vs SSD1306 / wiring.");
  } else {
    ui.setFlipped(settings.screenFlipped);
  }

  sensorHub.begin();
  sensorHub.setMountRotation(settings.mpuMountRotation);
  sensorHub.setGazeCalibration(settings.mpuGazeOffsetX, settings.mpuGazeOffsetY);
  const SensorSnapshot& bootSensors = sensorHub.snapshot();
  Serial.print("MPU6050: "); Serial.println(bootSensors.mpuAvailable ? "OK" : "not found");
  Serial.print("BMP180: "); Serial.println(bootSensors.bmpAvailable ? "OK" : "not found");

  bleConfig.begin(settings, settingsStore, petState, petStateStore);
  ui.setAnimationTuning(effectiveIdleSpeed(), settings.heartParticleCount, settings.skinPersonalityEnabled);
  scheduleNextMood(millis());
  scheduleRareEvent(millis());
  lastNeedsAt = millis();
  lastButtonActivityAt = millis();
  lastCompanionActivityAt = millis();
  previousPersonality = settings.personality;
  bootStartedAt = millis();
  previousSleepState = isSleepWindow(bootStartedAt);

  Serial.println("XIAO Computer Pet ready");
  Serial.println("BLE config service active: XIAO Computer Pet");
}

void loop() {
  const uint32_t now = millis();
  loopCounter++;
  behavior.update(now);

  bleConfig.update();
  if (lastSensorUpdateAt == 0 || (now - lastSensorUpdateAt) >= app::SENSOR_TASK_MS) {
    sensorHub.update(now);
    lastSensorUpdateAt = now;
  }
  const SensorSnapshot& sensorData = sensorHub.snapshot();
  if (sensorData.motion > 0.13f) lastCompanionActivityAt = now;
  bleConfig.setSensorTelemetry(sensorData);
  if (diagnosticWindowAt == 0) diagnosticWindowAt = now;
  const uint32_t diagnosticElapsed = now - diagnosticWindowAt;
  if (diagnosticElapsed >= app::BLE_DIAGNOSTIC_MS) {
    measuredLoopHz = diagnosticElapsed ? (static_cast<float>(loopCounter) * 1000.0f / static_cast<float>(diagnosticElapsed)) : 0.0f;
    measuredRenderFps = diagnosticElapsed ? (static_cast<float>(renderedFrameCounter) * 1000.0f / static_cast<float>(diagnosticElapsed)) : 0.0f;
    bleConfig.setRuntimeDiagnostics(measuredLoopHz, measuredRenderFps, ui.framesPresented(), ui.framesSkipped());
    loopCounter = 0;
    renderedFrameCounter = 0;
    diagnosticWindowAt = now;
  }
  const TouchEvents physicalLeftTouch = touchLeft.update(now);
  const TouchEvents physicalRightTouch = touchRight.update(now);
  const TouchEvents leftTouch = settings.swapTouchButtons ? physicalRightTouch : physicalLeftTouch;
  const TouchEvents rightTouch = settings.swapTouchButtons ? physicalLeftTouch : physicalRightTouch;
  const bool leftTouched = settings.swapTouchButtons ? touchRight.isTouched() : touchLeft.isTouched();
  const bool rightTouched = settings.swapTouchButtons ? touchLeft.isTouched() : touchRight.isTouched();
  const uint32_t leftTouchedFor = settings.swapTouchButtons ? touchRight.touchedFor(now) : touchLeft.touchedFor(now);
  const uint32_t rightTouchedFor = settings.swapTouchButtons ? touchLeft.touchedFor(now) : touchRight.touchedFor(now);

  AudioPreviewType previewType;
  uint8_t previewId;
  if (bleConfig.consumeAudioPreview(previewType, previewId)) {
    buzzer.stop();
    if (previewType == AudioPreviewType::Pet) buzzer.previewPetChirp(previewId);
    else if (previewType == AudioPreviewType::Done) buzzer.previewDoneMelody(previewId);
    else if (previewType == AudioPreviewType::Break) buzzer.previewBreakMelody(previewId);
    else if (previewType == AudioPreviewType::Start) buzzer.previewTimerStart();
  }

  buzzer.update(now);
  const ButtonEvents ev = button.update(now, settings.longPressMs, settings.doubleClickMs);

  // Auto-dim is driven only by physical controls, as requested. Touching any
  // control wakes the OLED immediately; after the configured idle delay the
  // controller contrast drops to its minimum value without stopping animation.
  if (ev.pressed || physicalLeftTouch.pressed || physicalRightTouch.pressed) {
    lastButtonActivityAt = now;
    lastCompanionActivityAt = now;
      if (speechActive) {
      speechActive = false;
      speechText[0] = '\0';
      speechCooldownUntil = now + 500UL;
    }
    if (oledDimmed) {
      oledDimmed = false;
      ui.setContrast(settings.oledContrast);
    }
  }
  const bool shouldDim = settings.idleDimSeconds > 0 &&
    (now - lastButtonActivityAt) >= static_cast<uint32_t>(settings.idleDimSeconds) * 1000UL;
  if (shouldDim != oledDimmed) {
    oledDimmed = shouldDim;
    ui.setContrast(oledDimmed ? 1 : settings.oledContrast);
  }

  uint32_t epoch;
  int16_t offset;
  if (bleConfig.consumeTimeSync(epoch, offset)) {
    syncEpoch = epoch;
    syncAtMs = now;
    tzOffsetMinutes = offset;
    clockValid = true;
    updateToday(now);
  }

  bool resetMpuCalibration = false;
  if (bleConfig.consumeMpuCalibrationRequest(resetMpuCalibration)) {
    bool ok = sensorData.mpuAvailable;
    if (ok && resetMpuCalibration) {
      settings.mpuGazeOffsetX = 0.0f;
      settings.mpuGazeOffsetY = 0.0f;
      sensorHub.resetCalibration();
    } else if (ok) {
      float offsetX = 0.0f, offsetY = 0.0f;
      ok = sensorHub.calibrateCurrent(offsetX, offsetY);
      if (ok) {
        settings.mpuGazeOffsetX = offsetX;
        settings.mpuGazeOffsetY = offsetY;
      }
    }
    if (ok) settingsStore.save(settings);
    bleConfig.reportMpuCalibration(ok, resetMpuCalibration);
  }

  if (bleConfig.consumeSettingsChanged()) {
    if (settings.personality != previousPersonality) {
      skinChangeStarted = now;
      previousPersonality = settings.personality;
      behavior.request(PetMood::Surprised, now, 520, BehaviorPriority::User, true, false);
    }
    buzzer.setEnabled(settings.soundEnabled);
    ui.setContrast(oledDimmed ? 1 : settings.oledContrast);
    ui.setFlipped(settings.screenFlipped);
    sensorHub.setMountRotation(settings.mpuMountRotation);
    sensorHub.setGazeCalibration(settings.mpuGazeOffsetX, settings.mpuGazeOffsetY);
    ui.setAnimationTuning(effectiveIdleSpeed(), settings.heartParticleCount, settings.skinPersonalityEnabled);
    scheduleNextMood(now);
    scheduleRareEvent(now);
  }

  updateToday(now);
  tickNeeds(now);
  flushPetStateIfDue(now);

  const bool sleepingNow = isSleepWindow(now);
  if (clockValid && sleepingNow != previousSleepState) {
    queueSpeech(sleepingNow ? SpeechContext::Sleep : SpeechContext::Wake, now + 120UL, true);
    previousSleepState = sleepingNow;
  }
  if (!bootGreetingDone && !sleepingNow && (now - bootStartedAt) >= 15000UL) {
    bootGreetingDone = true;
    queueSpeech(SpeechContext::Boot, now, true);
  }
  updateSpeech(now);

  const bool inputSuppressed = static_cast<int32_t>(now - suppressInputUntil) < 0;

  if (mode == AppMode::Pet && settings.deskBuddyEnabled && !petEffectActive && !inputSuppressed) {
    if (settings.motionReactionsEnabled) {
      const bool fallReaction = sensorHub.consumeFall();
      if (fallReaction) {
        // A drop has higher priority than passive motion reactions. Clear any
        // speech bubble so the X eyes are visible immediately and extend the
        // reaction again when the landing impact is detected.
        speechActive = false;
        speechText[0] = '\0';
        pendingSpeechContext = SpeechContext::None;
        triggerDeskMood(PetMood::Knocked, now, 1350, true, BehaviorPriority::Critical);
        strongShakeAwaitingSettle = false;
      }
      const bool knockedReactionActive = fallReaction ||
        (behavior.active(now) && behavior.mood() == PetMood::Knocked && behavior.explicitInteraction());

      // Always consume the shake flag, but don't let the same landing impact
      // queue a Dizzy reaction behind the higher-priority X-eye fall reaction.
      const bool shakeReaction = sensorHub.consumeShake();
      if (shakeReaction && !knockedReactionActive) {
        strongShakeAwaitingSettle = true;
        strongShakeAt = now;
      }
      if (strongShakeAwaitingSettle && (now - strongShakeAt) > 2500) strongShakeAwaitingSettle = false;

      const bool pickupReaction = sensorHub.consumePickup();
      if (pickupReaction && !knockedReactionActive) {
        triggerDeskMood(PetMood::Surprised, now, 420, false, BehaviorPriority::Physical);
        queueSpeech(SpeechContext::Pickup, now + 520UL);
      }

      const bool settledReaction = sensorHub.consumeSettled();
      if (settledReaction && !knockedReactionActive) {
        if (strongShakeAwaitingSettle) {
          triggerDeskMood(PetMood::Dizzy, now, 700, false, BehaviorPriority::Physical);
          queueSpeech(SpeechContext::Shake, now + 820UL);
          strongShakeAwaitingSettle = false;
        } else {
          triggerDeskMood(PetMood::Happy, now, 320, false, BehaviorPriority::Physical);
          queueSpeech(SpeechContext::Settled, now + 420UL);
        }
      }
    }

    const bool knockedReactionActiveNow = behavior.active(now) && behavior.mood() == PetMood::Knocked && behavior.explicitInteraction();
    if (settings.motionReactionsEnabled && sensorData.mpuAvailable && !knockedReactionActiveNow) {
      // Orientation must remain stable before an emotion changes. The continuous
      // gravity deformation already reacts immediately, so this layer stays subtle.
      const DeskOrientation orientation = classifyDeskOrientation(sensorData);
      if (orientation != orientationCandidate) {
        orientationCandidate = orientation;
        orientationCandidateSince = now;
      } else if (orientation != lastDeskOrientation && (now - orientationCandidateSince) >= 240) {
        lastDeskOrientation = orientation;
        if (orientation == DeskOrientation::Left || orientation == DeskOrientation::Right) {
          triggerDeskMood(PetMood::Curious, now, 420, false, BehaviorPriority::Gravity);
        } else if (orientation == DeskOrientation::FaceDown) {
          triggerDeskMood(PetMood::Sleepy, now, 520, false, BehaviorPriority::Gravity);
        }
      }
    }

    if (settings.touchReactionsEnabled) {
      if (leftTouch.pressed) lastLeftTouchAt = now;
      if (rightTouch.pressed) lastRightTouchAt = now;
      if (leftTouch.released) leftHoldActionFired = false;
      if (rightTouch.released) rightHoldActionFired = false;
      if (!leftTouched || !rightTouched) hugActionFired = false;

      if (settings.advancedTouchEnabled && static_cast<int32_t>(now - touchGestureCooldownUntil) >= 0) {
        // Two simultaneous touches = hug. A short left->right/right->left sequence = swipe.
        if (leftTouched && rightTouched && !hugActionFired &&
            leftTouchedFor > 120 && rightTouchedFor > 120) {
          hugActionFired = true;
          touchGestureCooldownUntil = now + 650;
          petState.hugs++;
          applyTouchReward(PetMood::Hug, now, 1200, 7, 6);
        } else if (leftTouch.released && rightTouched && lastRightTouchAt &&
                   abs(static_cast<int32_t>(lastLeftTouchAt - lastRightTouchAt)) > 60 &&
                   abs(static_cast<int32_t>(lastLeftTouchAt - lastRightTouchAt)) < 520) {
          touchGestureCooldownUntil = now + 500;
          petState.swipes++;
          applyTouchReward(PetMood::Affectionate, now, 950, 5, 4);
        } else if (rightTouch.released && leftTouched && lastLeftTouchAt &&
                   abs(static_cast<int32_t>(lastRightTouchAt - lastLeftTouchAt)) > 60 &&
                   abs(static_cast<int32_t>(lastRightTouchAt - lastLeftTouchAt)) < 520) {
          touchGestureCooldownUntil = now + 500;
          petState.swipes++;
          applyTouchReward(PetMood::Affectionate, now, 950, 5, 4);
        } else if (leftTouched && !rightTouched && !leftHoldActionFired && leftTouchedFor > 520) {
          leftHoldActionFired = true;
          petState.scratches++;
          applyTouchReward(PetMood::ScratchLeft, now, 900, 3, 3);
        } else if (rightTouched && !leftTouched && !rightHoldActionFired && rightTouchedFor > 520) {
          rightHoldActionFired = true;
          petState.scratches++;
          applyTouchReward(PetMood::ScratchRight, now, 900, 3, 3);
        } else if (leftTouch.pressed || rightTouch.pressed) {
          triggerDeskMood(PetMood::Curious, now, 500, true);
        }
      } else if (leftTouch.pressed || rightTouch.pressed) {
        triggerDeskMood(PetMood::Curious, now, 500, true);
      }
    }

    const bool quietForRareEvent = sensorData.motion < 0.08f && !leftTouched && !rightTouched && !button.isPressed() && !speechActive;
    if (settings.rareEventsEnabled && quietForRareEvent && static_cast<int32_t>(now - nextRareEventAt) >= 0 &&
        !behavior.active(now)) {
      triggerRareEvent(now);
    }
  }

  if (mode == AppMode::Pet) {
    if (!inputSuppressed && ev.longPress) {
      interaction = Interaction::None;
      enterPomodoroReady(TimerMode::Focus, now);
    } else if (!inputSuppressed && ev.doublePress) {
      // The first tap may already have started visual feedback, but a true
      // double-click is a menu gesture and must not count as a pet.
      openPetMenu(now);
    } else {
      if (!inputSuppressed && ev.tap) {
        // Start the animation immediately on release instead of waiting for
        // the double-click timeout.
        beginPetFeedback(now);
      }
      if (!inputSuppressed && ev.shortPress) {
        // Only commit stats once we know there was no second click.
        commitPetInteraction(now);
      }
    }

    if (petEffectActive && (now - petEffectStarted) >= settings.petAnimationMs) {
      const Interaction finishedInteraction = interaction;
      petEffectActive = false;
      interaction = Interaction::None;
      mood = PetMood::Idle;
      if (finishedInteraction == Interaction::Pet) {
        queueSpeech(SpeechContext::Pet, now + 100UL, true);
      }
    }

    if (!petEffectActive && !isSleepWindow(now) && static_cast<int32_t>(now - nextFaceAt) >= 0) {
      mood = randomMood();
      scheduleNextMood(now);
    }
  }

  else if (mode == AppMode::PomodoroReady) {
    if (!inputSuppressed && leftTouch.pressed && !rightTouch.pressed) {
      cycleTimerMode(-1, now);
    } else if (!inputSuppressed && rightTouch.pressed && !leftTouch.pressed) {
      cycleTimerMode(1, now);
    } else if (!inputSuppressed && ev.longPress) {
      returnToPet(now, PetMood::Idle);
    } else if (!inputSuppressed && ev.tap) {
      startSelectedTimer(now);
    }
  }

  else if (mode == AppMode::Pomodoro) {
    if (!inputSuppressed && ev.longPress) {
      pomodoro.cancel();
      returnToPet(now, PetMood::Idle);
    } else if (!inputSuppressed && (ev.shortPress || ev.doublePress)) {
      pomodoro.togglePause(now);
    }

    if (pomodoro.update(now)) {
      const bool wasBreak = pomodoro.isBreak();
      if (wasBreak) completeBreak();
      else completeFocus(now);
      beginCelebration(now, wasBreak);
    }
  }

  else if (mode == AppMode::Celebration) {
    if (ev.pressed) {
      buzzer.stop();
      const uint16_t suppressMs = (settings.longPressMs > settings.doubleClickMs) ? settings.longPressMs : settings.doubleClickMs;
      suppressInputUntil = now + suppressMs + 500UL;
      if (!celebrationWasBreak && settings.autoBreak) {
        const bool longBreak = (petState.focusSessions % settings.sessionsBeforeLongBreak) == 0;
        enterPomodoroReady(longBreak ? TimerMode::LongBreak : TimerMode::ShortBreak, now);
      } else {
        returnToPet(now, celebrationWasBreak ? PetMood::Idle : PetMood::Done);
        queueSpeech(celebrationWasBreak ? SpeechContext::BreakDone : SpeechContext::FocusDone, now + 500UL, true);
      }
    } else if (!buzzer.isPlaying() && (now - celebrationStarted) > 1300) {
      if (!celebrationWasBreak && settings.autoBreak) {
        const bool longBreak = (petState.focusSessions % settings.sessionsBeforeLongBreak) == 0;
        enterPomodoroReady(longBreak ? TimerMode::LongBreak : TimerMode::ShortBreak, now);
      } else {
        returnToPet(now, celebrationWasBreak ? PetMood::Idle : PetMood::Done);
        queueSpeech(celebrationWasBreak ? SpeechContext::BreakDone : SpeechContext::FocusDone, now + 500UL, true);
      }
    }
  }

  else if (mode == AppMode::PetMenu) {
    if (!inputSuppressed && ev.longPress) {
      backFromMenu(now);
    } else if (!inputSuppressed && leftTouch.pressed && !rightTouch.pressed) {
      moveMenu(-1, now);
    } else if (!inputSuppressed && rightTouch.pressed && !leftTouch.pressed) {
      moveMenu(1, now);
    } else if (!inputSuppressed && ev.tap) {
      // The top button is now dedicated to select/validate in menus.
      validateMenu(now);
    }
  }

  const uint8_t fps = constrain(settings.animationFps, 10, 60);
  const uint32_t frameMs = 1000UL / fps;
  if ((now - lastFrameAt) >= frameMs) {
    lastFrameAt = now;
    renderedFrameCounter++;

    if (mode == AppMode::Pet) {
      float p = 0.0f;
      if (petEffectActive && settings.petAnimationMs > 0) {
        p = static_cast<float>(now - petEffectStarted) / static_cast<float>(settings.petAnimationMs);
        if (p > 1.0f) p = 1.0f;
      }

      const bool scheduledSleep = isSleepWindow(now);
      uint32_t microSleepDelay = static_cast<uint32_t>(settings.microSleepSeconds) * 1000UL;
      if (settings.skinPersonalityEnabled) {
        microSleepDelay = static_cast<uint32_t>(
          (static_cast<uint64_t>(microSleepDelay) * petBehaviorProfile(settings.personality).microSleepScalePercent) / 100ULL);
      }
      const bool microSleeping = settings.microSleepEnabled && !scheduledSleep && !petEffectActive &&
        !speechActive && sensorData.motion < 0.05f && (now - lastCompanionActivityAt) >= microSleepDelay;
      const bool sleeping = scheduledSleep || microSleeping;
      const bool gravityDominant = settings.deskBuddyEnabled && sensorData.mpuAvailable &&
        sensorData.restCalibrated && (fabsf(sensorData.gazeX) > 0.18f || fabsf(sensorData.gazeY) > 0.18f);
      const bool knockedActive = behavior.active(now) && behavior.mood() == PetMood::Knocked && behavior.explicitInteraction();
      const bool bootAnimating = settings.bootAnimationEnabled && (now - bootStartedAt) < 2200UL;
      PetMood renderMood = knockedActive ? PetMood::Knocked : (sleeping ? PetMood::Sleepy : mood);
      if (bootAnimating && !knockedActive) {
        const uint32_t bp = now - bootStartedAt;
        renderMood = bp < 420 ? PetMood::Blink : (bp < 1200 ? PetMood::Curious : PetMood::Idle);
      }
      const bool deskMoodActive = (!sleeping || knockedActive) && !petEffectActive && behavior.active(now);
      if (deskMoodActive && (!gravityDominant || behavior.explicitInteraction())) {
        renderMood = behavior.mood();
        p = behavior.progress(now);
      } else if (gravityDominant && !petEffectActive && !sleeping) {
        // Gravity deformation owns the face while the device is clearly tilted.
        // This prevents low-priority autonomous emotions from fighting the pose.
        renderMood = PetMood::Idle;
        p = 0.0f;
      }
      if (interaction == Interaction::Boop && petEffectActive) {
        renderMood = (p < 0.35f) ? PetMood::Surprised : PetMood::Excited;
      } else if (interaction == Interaction::Pet && petEffectActive) {
        renderMood = PetMood::Affectionate;
      }

      float pressAmount = 0.0f;
      if (button.isPressed() && !button.longPressTriggered()) {
        pressAmount = static_cast<float>(button.pressedFor(now)) / 180.0f;
        if (pressAmount > 1.0f) pressAmount = 1.0f;
      }
      if (skinChangeStarted != 0 && (now - skinChangeStarted) < 520UL) {
        const float st = static_cast<float>(now - skinChangeStarted) / 520.0f;
        const float swapSquash = sinf(st * 3.1415926f) * 0.72f;
        if (swapSquash > pressAmount) pressAmount = swapSquash;
      }
      PetMotionInput petMotion;
      if (settings.deskBuddyEnabled && sensorData.mpuAvailable) {
        const float eyeFollow = static_cast<float>(settings.eyeFollowStrength) / 100.0f;
        petMotion.tiltX = constrain(sensorData.gazeX * eyeFollow, -1.0f, 1.0f);
        petMotion.tiltY = constrain(sensorData.gazeY * eyeFollow, -1.0f, 1.0f);
        petMotion.motion = settings.motionReactionsEnabled ? sensorData.motion : 0.0f;
        if (settings.motionReactionsEnabled) {
          const float sensitivity = static_cast<float>(settings.motionSensitivity) / 100.0f;
          petMotion.motionX = constrain(sensorData.motionX * sensitivity, -1.0f, 1.0f);
          petMotion.motionY = constrain(sensorData.motionY * sensitivity, -1.0f, 1.0f);
          petMotion.angularZ = constrain(sensorData.gyroZ / 6.0f * sensitivity, -1.0f, 1.0f);
        }
        petMotion.rotationSpeed = sqrtf(sensorData.gyroX * sensorData.gyroX + sensorData.gyroY * sensorData.gyroY + sensorData.gyroZ * sensorData.gyroZ);
        petMotion.gravityZ = sensorData.gravityZ;
        petMotion.inertiaStrength = static_cast<float>(settings.inertiaStrength) / 100.0f;
        petMotion.squashStrength = static_cast<float>(settings.squashStrength) / 100.0f;
      }
      const bool gravityPoseActive = settings.deskBuddyEnabled && sensorData.mpuAvailable &&
        sensorData.restCalibrated && (fabsf(sensorData.gazeX) > 0.16f || fabsf(sensorData.gazeY) > 0.16f);
      if (settings.deskBuddyEnabled && settings.touchReactionsEnabled && !gravityPoseActive) {
        if (leftTouched && !rightTouched) petMotion.tiltX = -1.0f;
        else if (rightTouched && !leftTouched) petMotion.tiltX = 1.0f;
        else if (leftTouched && rightTouched) petMotion.tiltY = -0.35f;
      }
      float speechProgress = 0.0f;
      if (speechActive && speechDuration > 0) {
        speechProgress = constrain(static_cast<float>(now - speechStarted) / static_cast<float>(speechDuration), 0.0f, 1.0f);
      }
      ui.renderPet(renderMood, now, p, settings.personality, activeAccessory(), pressAmount, petMotion,
                   speechActive ? speechText : nullptr, speechProgress, sleeping && renderMood != PetMood::Knocked);
    } else if (mode == AppMode::PomodoroReady) {
      float modeTransition = static_cast<float>(now - timerModeTransitionStarted) / static_cast<float>(TIMER_MODE_TRANSITION_MS);
      if (modeTransition > 1.0f) modeTransition = 1.0f;
      ui.renderPomodoroReady(
        static_cast<uint8_t>(selectedTimerMode),
        static_cast<uint8_t>(previousTimerMode),
        timerModeMinutes(selectedTimerMode),
        timerModeMinutes(previousTimerMode),
        modeTransition,
        timerModeDirection,
        sensorData.temperatureC,
        sensorData.pressureHpa,
        sensorData.bmpAvailable,
        now
      );
    } else if (mode == AppMode::Pomodoro) {
      ui.renderPomodoro(
        pomodoro.remainingMs(),
        pomodoro.totalMs(),
        pomodoro.paused(),
        static_cast<uint8_t>(selectedTimerMode),
        sensorData.temperatureC,
        sensorData.pressureHpa,
        sensorData.bmpAvailable,
        settings.focusCompanionEnabled,
        settings.timerCompanionLayout,
        settings.personality,
        now
      );
    } else if (mode == AppMode::PetMenu) {
      float transition = static_cast<float>(now - menuTransitionStarted) / static_cast<float>(MENU_TRANSITION_MS);
      if (transition > 1.0f) transition = 1.0f;
      float holdProgress = 0.0f;
      if (button.isPressed() && !button.longPressTriggered() && settings.longPressMs > 0) {
        holdProgress = static_cast<float>(button.pressedFor(now)) / static_cast<float>(settings.longPressMs);
        if (holdProgress > 1.0f) holdProgress = 1.0f;
      }
      ui.renderPetMenu(
        menuView, menuIndex, menuPreviousIndex, transition, menuDirection, now,
        petState, highestUnlockedAccessory(), holdProgress
      );
    } else {
      ui.renderPet(
        celebrationWasBreak ? PetMood::Happy : PetMood::Done,
        now,
        0.5f,
        settings.personality,
        activeAccessory()
      );
    }
  }
}
