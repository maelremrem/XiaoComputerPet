#include "PetStateStore.h"
#include "ProjectConfig.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <ArduinoJson.h>

using namespace Adafruit_LittleFS_Namespace;

void PetStateStore::sanitize(PetState& s) {
  s.version = 4;
  s.mood = constrain(s.mood, 0, 100);
  s.energy = constrain(s.energy, 0, 100);
  s.affection = constrain(s.affection, 0, 100);
  s.boredom = 0;
  s.level = static_cast<uint16_t>((s.xp / 100UL) + 1UL);
  if (s.level > 999) s.level = 999;
}

bool PetStateStore::load(PetState& s) {
  File file(InternalFS);
  if (!file.open(app::STATE_FILE, FILE_O_READ)) {
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

  s.mood = doc["mood"] | s.mood;
  s.energy = doc["energy"] | s.energy;
  s.affection = doc["affection"] | s.affection;
  s.xp = doc["xp"] | s.xp;
  s.pets = doc["pets"] | s.pets;
  s.boops = doc["boops"] | s.boops;
  s.focusSessions = doc["focusSessions"] | s.focusSessions;
  s.focusMinutes = doc["focusMinutes"] | s.focusMinutes;
  s.focusXp = doc["focusXp"] | s.focusXp;
  s.hugs = doc["hugs"] | s.hugs;
  s.scratches = doc["scratches"] | s.scratches;
  s.swipes = doc["swipes"] | s.swipes;
  s.rareEvents = doc["rareEvents"] | s.rareEvents;
  s.streakDays = doc["streakDays"] | s.streakDays;
  s.lastFocusDay = doc["lastFocusDay"] | s.lastFocusDay;
  s.todayDay = doc["todayDay"] | s.todayDay;
  s.todaySessions = doc["todaySessions"] | s.todaySessions;
  s.todayMinutes = doc["todayMinutes"] | s.todayMinutes;
  JsonArray historyDay = doc["historyDay"].as<JsonArray>();
  JsonArray historySessions = doc["historySessions"].as<JsonArray>();
  JsonArray historyMinutes = doc["historyMinutes"].as<JsonArray>();
  for (uint8_t i = 0; i < 7; ++i) {
    if (!historyDay.isNull() && i < historyDay.size()) s.historyDay[i] = historyDay[i] | s.historyDay[i];
    if (!historySessions.isNull() && i < historySessions.size()) s.historySessions[i] = historySessions[i] | s.historySessions[i];
    if (!historyMinutes.isNull() && i < historyMinutes.size()) s.historyMinutes[i] = historyMinutes[i] | s.historyMinutes[i];
  }
  sanitize(s);
  return true;
}

bool PetStateStore::save(const PetState& source) {
  PetState s = source;
  sanitize(s);

  InternalFS.remove(app::STATE_FILE);
  File file(InternalFS);
  if (!file.open(app::STATE_FILE, FILE_O_WRITE)) return false;

  JsonDocument doc;
  doc["version"] = s.version;
  doc["mood"] = s.mood;
  doc["energy"] = s.energy;
  doc["affection"] = s.affection;
  doc["xp"] = s.xp;
  doc["pets"] = s.pets;
  doc["boops"] = s.boops;
  doc["focusSessions"] = s.focusSessions;
  doc["focusMinutes"] = s.focusMinutes;
  doc["focusXp"] = s.focusXp;
  doc["hugs"] = s.hugs;
  doc["scratches"] = s.scratches;
  doc["swipes"] = s.swipes;
  doc["rareEvents"] = s.rareEvents;
  doc["streakDays"] = s.streakDays;
  doc["lastFocusDay"] = s.lastFocusDay;
  doc["todayDay"] = s.todayDay;
  doc["todaySessions"] = s.todaySessions;
  doc["todayMinutes"] = s.todayMinutes;
  JsonArray historyDay = doc["historyDay"].to<JsonArray>();
  JsonArray historySessions = doc["historySessions"].to<JsonArray>();
  JsonArray historyMinutes = doc["historyMinutes"].to<JsonArray>();
  for (uint8_t i = 0; i < 7; ++i) {
    historyDay.add(s.historyDay[i]);
    historySessions.add(s.historySessions[i]);
    historyMinutes.add(s.historyMinutes[i]);
  }
  serializeJson(doc, file);
  file.flush();
  file.close();
  return true;
}

bool PetStateStore::factoryReset(PetState& s) {
  s = PetState{};
  sanitize(s);
  return save(s);
}
