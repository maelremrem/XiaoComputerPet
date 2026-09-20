#include "SettingsStore.h"
#include "ProjectConfig.h"
#include "PetVisual.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <ArduinoJson.h>
#include <cstring>
#include <math.h>

using namespace Adafruit_LittleFS_Namespace;

namespace {
void sanitizeTwoWordText(char* text, size_t capacity, const char* fallback) {
  if (!text || capacity == 0) return;
  if (text[0] == '\0') {
    strncpy(text, fallback, capacity - 1);
    text[capacity - 1] = '\0';
  }

  char out[20] = {};
  size_t o = 0;
  uint8_t words = 0;
  bool inWord = false;
  for (size_t i = 0; text[i] != '\0' && o + 1 < sizeof(out) && o + 1 < capacity; ++i) {
    char c = text[i];
    const bool space = c == ' ' || c == '\t' || c == '\n' || c == '\r';
    if (space) {
      if (inWord) {
        inWord = false;
        if (words >= 2) break;
        if (o > 0 && out[o - 1] != ' ') out[o++] = ' ';
      }
      continue;
    }
    if (!inWord) {
      words++;
      if (words > 2) break;
      inWord = true;
    }
    out[o++] = c;
  }
  while (o > 0 && out[o - 1] == ' ') --o;
  out[o] = '\0';
  if (out[0] == '\0') {
    strncpy(out, fallback, sizeof(out) - 1);
    out[sizeof(out) - 1] = '\0';
  }
  strncpy(text, out, capacity - 1);
  text[capacity - 1] = '\0';
}
}

bool SettingsStore::begin() {
  return InternalFS.begin();
}

void SettingsStore::sanitize(Settings& s) {
  s.version = app::CONFIG_VERSION;
  s.focusMinutes = constrain(s.focusMinutes, 1, 180);
  s.breakMinutes = constrain(s.breakMinutes, 1, 60);
  s.longBreakMinutes = constrain(s.longBreakMinutes, 1, 120);
  s.sessionsBeforeLongBreak = constrain(s.sessionsBeforeLongBreak, 1, 12);
  s.faceMinSeconds = constrain(s.faceMinSeconds, 1, 300);
  s.faceMaxSeconds = constrain(s.faceMaxSeconds, s.faceMinSeconds, 600);
  s.animationFps = constrain(s.animationFps, 10, 60);
  s.longPressMs = constrain(s.longPressMs, 400, 3000);
  s.doubleClickMs = constrain(s.doubleClickMs, 180, 700);
  s.petAnimationMs = constrain(s.petAnimationMs, 400, 5000);
  s.oledContrast = constrain(s.oledContrast, 1, 255);
  if (s.idleDimSeconds != 0) s.idleDimSeconds = constrain(s.idleDimSeconds, 5, 600);
  s.petSound = constrain(s.petSound, 0, 2);
  s.doneMelody = constrain(s.doneMelody, 0, 3);
  s.breakMelody = constrain(s.breakMelody, 0, 2);
  s.personality = constrain(s.personality, 0, PET_SKIN_COUNT - 1);
  if (s.accessoryMode != 255) s.accessoryMode = constrain(s.accessoryMode, 0, 3);
  if (!isfinite(s.mpuGazeOffsetX)) s.mpuGazeOffsetX = 0.0f;
  if (!isfinite(s.mpuGazeOffsetY)) s.mpuGazeOffsetY = 0.0f;
  s.mpuGazeOffsetX = constrain(s.mpuGazeOffsetX, -1.5f, 1.5f);
  s.mpuGazeOffsetY = constrain(s.mpuGazeOffsetY, -1.5f, 1.5f);
  s.mpuMountRotation = constrain(s.mpuMountRotation, 0, 3);
  s.motionSensitivity = constrain(s.motionSensitivity, 50, 150);
  s.timerCompanionLayout = constrain(s.timerCompanionLayout, 0, 1);
  s.microSleepSeconds = constrain(s.microSleepSeconds, 30, 3600);
  s.speechPack = constrain(s.speechPack, 0, 4);
  s.rareEventMinSeconds = constrain(s.rareEventMinSeconds, 20, 1800);
  s.rareEventMaxSeconds = constrain(s.rareEventMaxSeconds, s.rareEventMinSeconds, 3600);
  s.idleAnimationSpeed = constrain(s.idleAnimationSpeed, 50, 150);
  s.eyeFollowStrength = constrain(s.eyeFollowStrength, 50, 150);
  s.inertiaStrength = constrain(s.inertiaStrength, 50, 150);
  s.squashStrength = constrain(s.squashStrength, 0, 150);
  s.heartParticleCount = constrain(s.heartParticleCount, 0, 16);
  s.speechEventChance = constrain(s.speechEventChance, 0, 100);
  s.sleepStartHour = constrain(s.sleepStartHour, 0, 23);
  s.sleepEndHour = constrain(s.sleepEndHour, 0, 23);
  sanitizeTwoWordText(s.customBootText, sizeof(s.customBootText), "Hello!");
  sanitizeTwoWordText(s.customPetText, sizeof(s.customPetText), "Happy Happy");
  if (s.petName[0] == '\0') strncpy(s.petName, "PIXEL", sizeof(s.petName));
  s.petName[sizeof(s.petName) - 1] = '\0';
}

bool SettingsStore::load(Settings& s) {
  File file(InternalFS);
  if (!file.open(app::CONFIG_FILE, FILE_O_READ)) {
    sanitize(s);
    return save(s);
  }

  JsonDocument doc;
  const auto err = deserializeJson(doc, file);
  file.close();
  if (err) {
    sanitize(s);
    return save(s);
  }

  s.focusMinutes = doc["focusMinutes"] | s.focusMinutes;
  s.breakMinutes = doc["breakMinutes"] | s.breakMinutes;
  s.longBreakMinutes = doc["longBreakMinutes"] | s.longBreakMinutes;
  s.sessionsBeforeLongBreak = doc["sessionsBeforeLongBreak"] | s.sessionsBeforeLongBreak;
  s.autoBreak = doc["autoBreak"] | s.autoBreak;
  s.faceMinSeconds = doc["faceMinSeconds"] | s.faceMinSeconds;
  s.faceMaxSeconds = doc["faceMaxSeconds"] | s.faceMaxSeconds;
  s.animationFps = doc["animationFps"] | s.animationFps;
  s.longPressMs = doc["longPressMs"] | s.longPressMs;
  s.doubleClickMs = doc["doubleClickMs"] | s.doubleClickMs;
  s.petAnimationMs = doc["petAnimationMs"] | s.petAnimationMs;
  s.oledContrast = doc["oledContrast"] | s.oledContrast;
  s.idleDimSeconds = doc["idleDimSeconds"] | s.idleDimSeconds;
  s.screenFlipped = doc["screenFlipped"] | s.screenFlipped;
  s.soundEnabled = doc["soundEnabled"] | s.soundEnabled;
  s.petSound = doc["petSound"] | s.petSound;
  s.doneMelody = doc["doneMelody"] | s.doneMelody;
  s.breakMelody = doc["breakMelody"] | s.breakMelody;
  s.personality = doc["personality"] | s.personality;
  s.accessoryMode = doc["accessoryMode"] | s.accessoryMode;
  s.sleepEnabled = doc["sleepEnabled"] | s.sleepEnabled;
  s.deskBuddyEnabled = doc["deskBuddyEnabled"] | s.deskBuddyEnabled;
  s.touchReactionsEnabled = doc["touchReactionsEnabled"] | s.touchReactionsEnabled;
  s.motionReactionsEnabled = doc["motionReactionsEnabled"] | s.motionReactionsEnabled;
  s.environmentReactionsEnabled = doc["environmentReactionsEnabled"] | s.environmentReactionsEnabled;
  s.advancedTouchEnabled = doc["advancedTouchEnabled"] | s.advancedTouchEnabled;
  s.rareEventsEnabled = doc["rareEventsEnabled"] | s.rareEventsEnabled;
  s.focusCompanionEnabled = doc["focusCompanionEnabled"] | s.focusCompanionEnabled;
  s.timerCompanionLayout = doc["timerCompanionLayout"] | s.timerCompanionLayout;
  s.microSleepEnabled = doc["microSleepEnabled"] | s.microSleepEnabled;
  s.microSleepSeconds = doc["microSleepSeconds"] | s.microSleepSeconds;
  s.skinPersonalityEnabled = doc["skinPersonalityEnabled"] | s.skinPersonalityEnabled;
  s.bootAnimationEnabled = doc["bootAnimationEnabled"] | s.bootAnimationEnabled;
  s.speechPack = doc["speechPack"] | s.speechPack;
  s.speechBubblesEnabled = doc["speechBubblesEnabled"] | s.speechBubblesEnabled;
  s.speechEventChance = doc["speechEventChance"] | s.speechEventChance;
  s.rareEventMinSeconds = doc["rareEventMinSeconds"] | s.rareEventMinSeconds;
  s.rareEventMaxSeconds = doc["rareEventMaxSeconds"] | s.rareEventMaxSeconds;
  s.idleAnimationSpeed = doc["idleAnimationSpeed"] | s.idleAnimationSpeed;
  s.eyeFollowStrength = doc["eyeFollowStrength"] | s.eyeFollowStrength;
  s.inertiaStrength = doc["inertiaStrength"] | s.inertiaStrength;
  s.squashStrength = doc["squashStrength"] | s.squashStrength;
  s.heartParticleCount = doc["heartParticleCount"] | s.heartParticleCount;
  s.swapTouchButtons = doc["swapTouchButtons"] | s.swapTouchButtons;
  s.mpuGazeOffsetX = doc["mpuGazeOffsetX"] | s.mpuGazeOffsetX;
  s.mpuGazeOffsetY = doc["mpuGazeOffsetY"] | s.mpuGazeOffsetY;
  s.mpuMountRotation = doc["mpuMountRotation"] | s.mpuMountRotation;
  s.motionSensitivity = doc["motionSensitivity"] | s.motionSensitivity;
  s.sleepStartHour = doc["sleepStartHour"] | s.sleepStartHour;
  s.sleepEndHour = doc["sleepEndHour"] | s.sleepEndHour;

  const char* bootText = doc["customBootText"] | s.customBootText;
  strncpy(s.customBootText, bootText, sizeof(s.customBootText) - 1);
  s.customBootText[sizeof(s.customBootText) - 1] = '\0';
  const char* petText = doc["customPetText"] | s.customPetText;
  strncpy(s.customPetText, petText, sizeof(s.customPetText) - 1);
  s.customPetText[sizeof(s.customPetText) - 1] = '\0';
  const char* name = doc["petName"] | s.petName;
  strncpy(s.petName, name, sizeof(s.petName) - 1);
  s.petName[sizeof(s.petName) - 1] = '\0';
  sanitize(s);
  return true;
}

bool SettingsStore::save(const Settings& source) {
  Settings s = source;
  sanitize(s);

  InternalFS.remove(app::CONFIG_FILE);
  File file(InternalFS);
  if (!file.open(app::CONFIG_FILE, FILE_O_WRITE)) return false;

  JsonDocument doc;
  doc["version"] = app::CONFIG_VERSION;
  doc["focusMinutes"] = s.focusMinutes;
  doc["breakMinutes"] = s.breakMinutes;
  doc["longBreakMinutes"] = s.longBreakMinutes;
  doc["sessionsBeforeLongBreak"] = s.sessionsBeforeLongBreak;
  doc["autoBreak"] = s.autoBreak;
  doc["faceMinSeconds"] = s.faceMinSeconds;
  doc["faceMaxSeconds"] = s.faceMaxSeconds;
  doc["animationFps"] = s.animationFps;
  doc["longPressMs"] = s.longPressMs;
  doc["doubleClickMs"] = s.doubleClickMs;
  doc["petAnimationMs"] = s.petAnimationMs;
  doc["oledContrast"] = s.oledContrast;
  doc["idleDimSeconds"] = s.idleDimSeconds;
  doc["screenFlipped"] = s.screenFlipped;
  doc["soundEnabled"] = s.soundEnabled;
  doc["petSound"] = s.petSound;
  doc["doneMelody"] = s.doneMelody;
  doc["breakMelody"] = s.breakMelody;
  doc["personality"] = s.personality;
  doc["accessoryMode"] = s.accessoryMode;
  doc["deskBuddyEnabled"] = s.deskBuddyEnabled;
  doc["touchReactionsEnabled"] = s.touchReactionsEnabled;
  doc["motionReactionsEnabled"] = s.motionReactionsEnabled;
  doc["environmentReactionsEnabled"] = s.environmentReactionsEnabled;
  doc["advancedTouchEnabled"] = s.advancedTouchEnabled;
  doc["rareEventsEnabled"] = s.rareEventsEnabled;
  doc["focusCompanionEnabled"] = s.focusCompanionEnabled;
  doc["timerCompanionLayout"] = s.timerCompanionLayout;
  doc["microSleepEnabled"] = s.microSleepEnabled;
  doc["microSleepSeconds"] = s.microSleepSeconds;
  doc["skinPersonalityEnabled"] = s.skinPersonalityEnabled;
  doc["bootAnimationEnabled"] = s.bootAnimationEnabled;
  doc["speechPack"] = s.speechPack;
  doc["customBootText"] = s.customBootText;
  doc["customPetText"] = s.customPetText;
  doc["speechBubblesEnabled"] = s.speechBubblesEnabled;
  doc["speechEventChance"] = s.speechEventChance;
  doc["rareEventMinSeconds"] = s.rareEventMinSeconds;
  doc["rareEventMaxSeconds"] = s.rareEventMaxSeconds;
  doc["idleAnimationSpeed"] = s.idleAnimationSpeed;
  doc["eyeFollowStrength"] = s.eyeFollowStrength;
  doc["inertiaStrength"] = s.inertiaStrength;
  doc["squashStrength"] = s.squashStrength;
  doc["heartParticleCount"] = s.heartParticleCount;
  doc["swapTouchButtons"] = s.swapTouchButtons;
  doc["mpuGazeOffsetX"] = s.mpuGazeOffsetX;
  doc["mpuGazeOffsetY"] = s.mpuGazeOffsetY;
  doc["mpuMountRotation"] = s.mpuMountRotation;
  doc["motionSensitivity"] = s.motionSensitivity;
  doc["sleepEnabled"] = s.sleepEnabled;
  doc["sleepStartHour"] = s.sleepStartHour;
  doc["sleepEndHour"] = s.sleepEndHour;
  doc["petName"] = s.petName;

  serializeJson(doc, file);
  file.flush();
  file.close();
  return true;
}

bool SettingsStore::factoryReset(Settings& s) {
  s = Settings{};
  sanitize(s);
  return save(s);
}
