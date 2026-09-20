#pragma once
#include <Arduino.h>
#include <bluefruit.h>
#include "Settings.h"
#include "SettingsStore.h"
#include "PetState.h"
#include "PetStateStore.h"
#include "SensorHub.h"

enum class AudioPreviewType : uint8_t { None, Pet, Done, Break, Start, Stop };

class BleConfigService {
 public:
  void begin(Settings& settings, SettingsStore& store, PetState& state, PetStateStore& stateStore);
  void update();
  bool consumeSettingsChanged();
  bool consumeTimeSync(uint32_t& epoch, int16_t& tzOffsetMinutes);
  bool consumeAudioPreview(AudioPreviewType& type, uint8_t& id);
  bool consumeMpuCalibrationRequest(bool& reset);
  void reportMpuCalibration(bool ok, bool reset);
  void setSensorTelemetry(const SensorSnapshot& sensors);
  void setRuntimeDiagnostics(float loopHz, float renderFps, uint32_t framesPresented, uint32_t framesSkipped);

 private:
  void handleLine(const String& line);
  void sendSettings();
  void sendState();
  void sendSensors();
  void sendDiagnostics();
  void sendStatus(const char* status, const char* detail = nullptr);

  BLEUart uart_;
  Settings* settings_ = nullptr;
  SettingsStore* store_ = nullptr;
  PetState* state_ = nullptr;
  PetStateStore* stateStore_ = nullptr;
  String rxLine_;
  bool changed_ = false;
  bool timeSyncPending_ = false;
  uint32_t syncedEpoch_ = 0;
  int16_t syncedTzOffsetMinutes_ = 0;
  bool audioPreviewPending_ = false;
  AudioPreviewType audioPreviewType_ = AudioPreviewType::None;
  uint8_t audioPreviewId_ = 0;
  SensorSnapshot sensors_;
  bool mpuCalibrationPending_ = false;
  bool mpuCalibrationReset_ = false;
  float loopHz_ = 0.0f;
  float renderFps_ = 0.0f;
  uint32_t framesPresented_ = 0;
  uint32_t framesSkipped_ = 0;
};
