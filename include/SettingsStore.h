#pragma once
#include "Settings.h"

class SettingsStore {
 public:
  bool begin();
  bool load(Settings& settings);
  bool save(const Settings& settings);
  bool factoryReset(Settings& settings);

 private:
  void sanitize(Settings& settings);
};
