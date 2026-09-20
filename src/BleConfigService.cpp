#include "BleConfigService.h"
#include "ProjectConfig.h"
#include <ArduinoJson.h>
#include <cstring>

void BleConfigService::begin(Settings& settings, SettingsStore& store, PetState& state, PetStateStore& stateStore) {
  settings_ = &settings;
  store_ = &store;
  state_ = &state;
  stateStore_ = &stateStore;
  rxLine_.reserve(768);

  // Allow several BLE packets per connection event. This noticeably reduces
  // command latency from the Web Bluetooth configurator.
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName(app::DEVICE_NAME);
  Bluefruit.Periph.setConnInterval(6, 12); // 7.5-15 ms target interval
  uart_.begin();
  uart_.bufferTXD(true);

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(uart_);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void BleConfigService::update() {
  while (uart_.available()) {
    const char c = static_cast<char>(uart_.read());
    if (c == '\n' || c == '\r') {
      if (rxLine_.length()) {
        handleLine(rxLine_);
        rxLine_ = "";
      }
    } else if (rxLine_.length() < 1100) {
      rxLine_ += c;
    } else {
      rxLine_ = "";
      sendStatus("error", "message_too_long");
    }
  }
}

bool BleConfigService::consumeSettingsChanged() {
  const bool out = changed_;
  changed_ = false;
  return out;
}

bool BleConfigService::consumeTimeSync(uint32_t& epoch, int16_t& tzOffsetMinutes) {
  if (!timeSyncPending_) return false;
  timeSyncPending_ = false;
  epoch = syncedEpoch_;
  tzOffsetMinutes = syncedTzOffsetMinutes_;
  return true;
}

bool BleConfigService::consumeAudioPreview(AudioPreviewType& type, uint8_t& id) {
  if (!audioPreviewPending_) return false;
  audioPreviewPending_ = false;
  type = audioPreviewType_;
  id = audioPreviewId_;
  audioPreviewType_ = AudioPreviewType::None;
  return true;
}

void BleConfigService::sendStatus(const char* status, const char* detail) {
  JsonDocument doc;
  doc["type"] = "status";
  doc["status"] = status;
  if (detail) doc["detail"] = detail;
  serializeJson(doc, uart_);
  uart_.println();
}

void BleConfigService::sendSettings() {
  JsonDocument doc;
  doc["type"] = "settings";
  doc["focusMinutes"] = settings_->focusMinutes;
  doc["breakMinutes"] = settings_->breakMinutes;
  doc["longBreakMinutes"] = settings_->longBreakMinutes;
  doc["sessionsBeforeLongBreak"] = settings_->sessionsBeforeLongBreak;
  doc["autoBreak"] = settings_->autoBreak;
  doc["faceMinSeconds"] = settings_->faceMinSeconds;
  doc["faceMaxSeconds"] = settings_->faceMaxSeconds;
  doc["animationFps"] = settings_->animationFps;
  doc["longPressMs"] = settings_->longPressMs;
  doc["doubleClickMs"] = settings_->doubleClickMs;
  doc["petAnimationMs"] = settings_->petAnimationMs;
  doc["oledContrast"] = settings_->oledContrast;
  doc["idleDimSeconds"] = settings_->idleDimSeconds;
  doc["screenFlipped"] = settings_->screenFlipped;
  doc["soundEnabled"] = settings_->soundEnabled;
  doc["petSound"] = settings_->petSound;
  doc["doneMelody"] = settings_->doneMelody;
  doc["breakMelody"] = settings_->breakMelody;
  doc["personality"] = settings_->personality;
  doc["accessoryMode"] = settings_->accessoryMode;
  doc["deskBuddyEnabled"] = settings_->deskBuddyEnabled;
  doc["touchReactionsEnabled"] = settings_->touchReactionsEnabled;
  doc["motionReactionsEnabled"] = settings_->motionReactionsEnabled;
  doc["environmentReactionsEnabled"] = settings_->environmentReactionsEnabled;
  doc["advancedTouchEnabled"] = settings_->advancedTouchEnabled;
  doc["rareEventsEnabled"] = settings_->rareEventsEnabled;
  doc["focusCompanionEnabled"] = settings_->focusCompanionEnabled;
  doc["timerCompanionLayout"] = settings_->timerCompanionLayout;
  doc["microSleepEnabled"] = settings_->microSleepEnabled;
  doc["microSleepSeconds"] = settings_->microSleepSeconds;
  doc["skinPersonalityEnabled"] = settings_->skinPersonalityEnabled;
  doc["bootAnimationEnabled"] = settings_->bootAnimationEnabled;
  doc["speechPack"] = settings_->speechPack;
  doc["customBootText"] = settings_->customBootText;
  doc["customPetText"] = settings_->customPetText;
  doc["speechBubblesEnabled"] = settings_->speechBubblesEnabled;
  doc["speechEventChance"] = settings_->speechEventChance;
  doc["rareEventMinSeconds"] = settings_->rareEventMinSeconds;
  doc["rareEventMaxSeconds"] = settings_->rareEventMaxSeconds;
  doc["idleAnimationSpeed"] = settings_->idleAnimationSpeed;
  doc["eyeFollowStrength"] = settings_->eyeFollowStrength;
  doc["inertiaStrength"] = settings_->inertiaStrength;
  doc["squashStrength"] = settings_->squashStrength;
  doc["heartParticleCount"] = settings_->heartParticleCount;
  doc["swapTouchButtons"] = settings_->swapTouchButtons;
  doc["mpuGazeOffsetX"] = settings_->mpuGazeOffsetX;
  doc["mpuGazeOffsetY"] = settings_->mpuGazeOffsetY;
  doc["mpuMountRotation"] = settings_->mpuMountRotation;
  doc["motionSensitivity"] = settings_->motionSensitivity;
  doc["sleepEnabled"] = settings_->sleepEnabled;
  doc["sleepStartHour"] = settings_->sleepStartHour;
  doc["sleepEndHour"] = settings_->sleepEndHour;
  doc["petName"] = settings_->petName;
  serializeJson(doc, uart_);
  uart_.println();
}

void BleConfigService::sendState() {
  JsonDocument doc;
  doc["type"] = "state";
  doc["mood"] = state_->mood;
  doc["energy"] = state_->energy;
  doc["affection"] = state_->affection;
  doc["xp"] = state_->xp;
  doc["level"] = state_->level;
  doc["pets"] = state_->pets;
  doc["boops"] = state_->boops;
  doc["focusSessions"] = state_->focusSessions;
  doc["focusMinutes"] = state_->focusMinutes;
  doc["focusXp"] = state_->focusXp;
  doc["hugs"] = state_->hugs;
  doc["scratches"] = state_->scratches;
  doc["swipes"] = state_->swipes;
  doc["rareEvents"] = state_->rareEvents;
  doc["streakDays"] = state_->streakDays;
  doc["todaySessions"] = state_->todaySessions;
  doc["todayMinutes"] = state_->todayMinutes;
  JsonArray historyDay = doc["historyDay"].to<JsonArray>();
  JsonArray historySessions = doc["historySessions"].to<JsonArray>();
  JsonArray historyMinutes = doc["historyMinutes"].to<JsonArray>();
  for (uint8_t i = 0; i < 7; ++i) {
    historyDay.add(state_->historyDay[i]);
    historySessions.add(state_->historySessions[i]);
    historyMinutes.add(state_->historyMinutes[i]);
  }
  serializeJson(doc, uart_);
  uart_.println();
}

void BleConfigService::setSensorTelemetry(const SensorSnapshot& sensors) {
  sensors_ = sensors;
}

void BleConfigService::sendSensors() {
  JsonDocument doc;
  doc["type"] = "sensors";
  doc["mpuAvailable"] = sensors_.mpuAvailable;
  doc["bmpAvailable"] = sensors_.bmpAvailable;
  doc["gazeX"] = sensors_.gazeX;
  doc["gazeY"] = sensors_.gazeY;
  doc["motion"] = sensors_.motion;
  doc["linearX"] = sensors_.linearX;
  doc["linearY"] = sensors_.linearY;
  doc["gravityX"] = sensors_.gravityX;
  doc["gravityY"] = sensors_.gravityY;
  doc["gravityZ"] = sensors_.gravityZ;
  doc["stillForMs"] = sensors_.stillForMs;
  doc["restCalibrated"] = sensors_.restCalibrated;
  doc["restCalProgressMs"] = sensors_.restCalProgressMs;
  if (sensors_.bmpAvailable) {
    doc["temperatureC"] = sensors_.temperatureC;
    doc["pressureHpa"] = sensors_.pressureHpa;
  }
  serializeJson(doc, uart_);
  uart_.println();
}

void BleConfigService::setRuntimeDiagnostics(float loopHz, float renderFps, uint32_t framesPresented, uint32_t framesSkipped) {
  loopHz_ = loopHz;
  renderFps_ = renderFps;
  framesPresented_ = framesPresented;
  framesSkipped_ = framesSkipped;
}

void BleConfigService::sendDiagnostics() {
  JsonDocument doc;
  doc["type"] = "diagnostics";
  doc["loopHz"] = loopHz_;
  doc["renderFps"] = renderFps_;
  doc["framesPresented"] = framesPresented_;
  doc["framesSkipped"] = framesSkipped_;
  doc["uptimeMs"] = millis();
  serializeJson(doc, uart_);
  uart_.println();
}

void BleConfigService::handleLine(const String& line) {
  // Tiny one-packet commands used by audio preview for minimum latency.
  // PpN = pet sound, PdN = focus melody, PbN = break melody, Pt0 = timer start, PX = stop.
  if (line == "PX") {
    audioPreviewType_ = AudioPreviewType::Stop;
    audioPreviewId_ = 0;
    audioPreviewPending_ = true;
    return;
  }
  if (line.length() == 3 && line[0] == 'P') {
    const char kind = line[1];
    const uint8_t id = static_cast<uint8_t>(line[2] - '0');
    if (kind == 'p' && id < 3) audioPreviewType_ = AudioPreviewType::Pet;
    else if (kind == 'd' && id < 4) audioPreviewType_ = AudioPreviewType::Done;
    else if (kind == 'b' && id < 3) audioPreviewType_ = AudioPreviewType::Break;
    else if (kind == 't' && id == 0) audioPreviewType_ = AudioPreviewType::Start;
    else return;
    audioPreviewId_ = id;
    audioPreviewPending_ = true;
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, line)) {
    sendStatus("error", "invalid_json");
    return;
  }

  const char* cmd = doc["cmd"] | "";
  if (!strcmp(cmd, "get")) {
    sendSettings();
    sendState();
    sendSensors();
    sendDiagnostics();
    return;
  }

  if (!strcmp(cmd, "state")) {
    sendState();
    return;
  }

  if (!strcmp(cmd, "sensors")) {
    sendSensors();
    return;
  }

  if (!strcmp(cmd, "diagnostics")) {
    sendDiagnostics();
    return;
  }

  if (!strcmp(cmd, "calibrate_mpu")) {
    mpuCalibrationReset_ = false;
    mpuCalibrationPending_ = true;
    sendStatus("ok", "mpu_calibration_requested");
    return;
  }

  if (!strcmp(cmd, "reset_mpu_calibration")) {
    mpuCalibrationReset_ = true;
    mpuCalibrationPending_ = true;
    sendStatus("ok", "mpu_calibration_reset_requested");
    return;
  }

  if (!strcmp(cmd, "time")) {
    syncedEpoch_ = doc["epoch"] | 0UL;
    syncedTzOffsetMinutes_ = doc["tzOffsetMinutes"] | 0;
    if (syncedEpoch_ > 0) {
      timeSyncPending_ = true;
      sendStatus("ok", "time_synced");
    } else {
      sendStatus("error", "invalid_time");
    }
    return;
  }

  if (!strcmp(cmd, "preview")) {
    const char* kind = doc["kind"] | "";
    const uint8_t id = doc["id"] | 0;

    if (!strcmp(kind, "pet") && id < 3) audioPreviewType_ = AudioPreviewType::Pet;
    else if (!strcmp(kind, "done") && id < 4) audioPreviewType_ = AudioPreviewType::Done;
    else if (!strcmp(kind, "break") && id < 3) audioPreviewType_ = AudioPreviewType::Break;
    else if (!strcmp(kind, "start") && id == 0) audioPreviewType_ = AudioPreviewType::Start;
    else {
      sendStatus("error", "invalid_preview");
      return;
    }

    audioPreviewId_ = id;
    audioPreviewPending_ = true;
    sendStatus("ok", "preview_started");
    return;
  }

  if (!strcmp(cmd, "preview_stop")) {
    audioPreviewType_ = AudioPreviewType::Stop;
    audioPreviewId_ = 0;
    audioPreviewPending_ = true;
    sendStatus("ok", "preview_stopped");
    return;
  }

  if (!strcmp(cmd, "factory")) {
    store_->factoryReset(*settings_);
    stateStore_->factoryReset(*state_);
    changed_ = true;
    sendStatus("ok", "factory_reset");
    sendSettings();
    sendState();
    return;
  }

  if (!strcmp(cmd, "set")) {
    settings_->focusMinutes = doc["focusMinutes"] | settings_->focusMinutes;
    settings_->breakMinutes = doc["breakMinutes"] | settings_->breakMinutes;
    settings_->longBreakMinutes = doc["longBreakMinutes"] | settings_->longBreakMinutes;
    settings_->sessionsBeforeLongBreak = doc["sessionsBeforeLongBreak"] | settings_->sessionsBeforeLongBreak;
    settings_->autoBreak = doc["autoBreak"] | settings_->autoBreak;
    settings_->faceMinSeconds = doc["faceMinSeconds"] | settings_->faceMinSeconds;
    settings_->faceMaxSeconds = doc["faceMaxSeconds"] | settings_->faceMaxSeconds;
    settings_->animationFps = doc["animationFps"] | settings_->animationFps;
    settings_->longPressMs = doc["longPressMs"] | settings_->longPressMs;
    settings_->doubleClickMs = doc["doubleClickMs"] | settings_->doubleClickMs;
    settings_->petAnimationMs = doc["petAnimationMs"] | settings_->petAnimationMs;
    settings_->oledContrast = doc["oledContrast"] | settings_->oledContrast;
    settings_->idleDimSeconds = doc["idleDimSeconds"] | settings_->idleDimSeconds;
    settings_->screenFlipped = doc["screenFlipped"] | settings_->screenFlipped;
    settings_->soundEnabled = doc["soundEnabled"] | settings_->soundEnabled;
    settings_->petSound = doc["petSound"] | settings_->petSound;
    settings_->doneMelody = doc["doneMelody"] | settings_->doneMelody;
    settings_->breakMelody = doc["breakMelody"] | settings_->breakMelody;
    settings_->personality = doc["personality"] | settings_->personality;
    settings_->accessoryMode = doc["accessoryMode"] | settings_->accessoryMode;
    settings_->deskBuddyEnabled = doc["deskBuddyEnabled"] | settings_->deskBuddyEnabled;
    settings_->touchReactionsEnabled = doc["touchReactionsEnabled"] | settings_->touchReactionsEnabled;
    settings_->motionReactionsEnabled = doc["motionReactionsEnabled"] | settings_->motionReactionsEnabled;
    settings_->environmentReactionsEnabled = doc["environmentReactionsEnabled"] | settings_->environmentReactionsEnabled;
    settings_->advancedTouchEnabled = doc["advancedTouchEnabled"] | settings_->advancedTouchEnabled;
    settings_->rareEventsEnabled = doc["rareEventsEnabled"] | settings_->rareEventsEnabled;
    settings_->focusCompanionEnabled = doc["focusCompanionEnabled"] | settings_->focusCompanionEnabled;
    settings_->timerCompanionLayout = doc["timerCompanionLayout"] | settings_->timerCompanionLayout;
    settings_->microSleepEnabled = doc["microSleepEnabled"] | settings_->microSleepEnabled;
    settings_->microSleepSeconds = doc["microSleepSeconds"] | settings_->microSleepSeconds;
    settings_->skinPersonalityEnabled = doc["skinPersonalityEnabled"] | settings_->skinPersonalityEnabled;
    settings_->bootAnimationEnabled = doc["bootAnimationEnabled"] | settings_->bootAnimationEnabled;
    settings_->speechPack = doc["speechPack"] | settings_->speechPack;
    settings_->speechBubblesEnabled = doc["speechBubblesEnabled"] | settings_->speechBubblesEnabled;
    settings_->speechEventChance = doc["speechEventChance"] | settings_->speechEventChance;
    settings_->rareEventMinSeconds = doc["rareEventMinSeconds"] | settings_->rareEventMinSeconds;
    settings_->rareEventMaxSeconds = doc["rareEventMaxSeconds"] | settings_->rareEventMaxSeconds;
    settings_->idleAnimationSpeed = doc["idleAnimationSpeed"] | settings_->idleAnimationSpeed;
    settings_->eyeFollowStrength = doc["eyeFollowStrength"] | settings_->eyeFollowStrength;
    settings_->inertiaStrength = doc["inertiaStrength"] | settings_->inertiaStrength;
    settings_->squashStrength = doc["squashStrength"] | settings_->squashStrength;
    settings_->heartParticleCount = doc["heartParticleCount"] | settings_->heartParticleCount;
    settings_->swapTouchButtons = doc["swapTouchButtons"] | settings_->swapTouchButtons;
    const uint8_t oldMpuMountRotation = settings_->mpuMountRotation;
    settings_->mpuMountRotation = doc["mpuMountRotation"] | settings_->mpuMountRotation;
    settings_->motionSensitivity = doc["motionSensitivity"] | settings_->motionSensitivity;
    if (settings_->mpuMountRotation != oldMpuMountRotation) {
      // Calibration is in screen coordinates, so a mount rotation change needs
      // a fresh neutral point rather than reusing incompatible offsets.
      settings_->mpuGazeOffsetX = 0.0f;
      settings_->mpuGazeOffsetY = 0.0f;
    }
    settings_->sleepEnabled = doc["sleepEnabled"] | settings_->sleepEnabled;
    settings_->sleepStartHour = doc["sleepStartHour"] | settings_->sleepStartHour;
    settings_->sleepEndHour = doc["sleepEndHour"] | settings_->sleepEndHour;

    if (doc["customBootText"].is<const char*>()) {
      const char* text = doc["customBootText"];
      strncpy(settings_->customBootText, text, sizeof(settings_->customBootText) - 1);
      settings_->customBootText[sizeof(settings_->customBootText) - 1] = '\0';
    }
    if (doc["customPetText"].is<const char*>()) {
      const char* text = doc["customPetText"];
      strncpy(settings_->customPetText, text, sizeof(settings_->customPetText) - 1);
      settings_->customPetText[sizeof(settings_->customPetText) - 1] = '\0';
    }

    if (doc["petName"].is<const char*>()) {
      const char* name = doc["petName"];
      strncpy(settings_->petName, name, sizeof(settings_->petName) - 1);
      settings_->petName[sizeof(settings_->petName) - 1] = '\0';
    }

    if (!store_->save(*settings_)) {
      sendStatus("error", "save_failed");
      return;
    }
    store_->load(*settings_);
    changed_ = true;
    sendStatus("ok", "saved");
    sendSettings();
    return;
  }

  sendStatus("error", "unknown_command");
}

bool BleConfigService::consumeMpuCalibrationRequest(bool& reset) {
  if (!mpuCalibrationPending_) return false;
  mpuCalibrationPending_ = false;
  reset = mpuCalibrationReset_;
  return true;
}

void BleConfigService::reportMpuCalibration(bool ok, bool reset) {
  if (!ok) {
    sendStatus("error", "mpu_not_found");
    return;
  }
  sendStatus("ok", reset ? "mpu_calibration_reset" : "mpu_calibrated");
  sendSettings();
}
