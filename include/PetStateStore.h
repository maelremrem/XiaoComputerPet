#pragma once
#include "PetState.h"

class PetStateStore {
 public:
  bool load(PetState& state);
  bool save(const PetState& state);
  bool factoryReset(PetState& state);
  static void sanitize(PetState& state);
};
