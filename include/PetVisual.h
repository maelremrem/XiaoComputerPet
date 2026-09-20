#pragma once
#include <Arduino.h>

enum class PetMood : uint8_t {
  Idle,
  Blink,
  Curious,
  Sleepy,
  Happy,
  Excited,
  Done,
  Surprised,
  Focused,
  Bored,
  Affectionate,
  Dizzy,
  Yawn,
  Sneeze,
  Dance,
  Dream,
  Hiccup,
  ScratchLeft,
  ScratchRight,
  Hug,
  Knocked
};

enum class PetEyeStyle : uint8_t {
  Rounded,
  Square,
  Pill,
  Pixel,
  Alien,
  Cat,
  Visor,
  Ring,
  Sleep,
  Scout,
  Bubble,
  Mask
};

enum class PetBrowStyle : uint8_t {
  Arc,
  Flat,
  Short,
  Floating
};

struct PetSkinDefinition {
  const char* name;
  const char* detail;
  int8_t leftX;
  int8_t rightX;
  int8_t eyeY;
  uint8_t eyeW;
  uint8_t eyeH;
  uint8_t radius;
  PetEyeStyle eyeStyle;
  PetBrowStyle browStyle;
  uint8_t browThickness;
  uint8_t flags;
};


struct PetBehaviorProfile {
  uint8_t blinkWeight;
  uint8_t curiousWeight;
  uint8_t happyWeight;
  uint8_t focusedWeight;
  uint8_t sleepyWeight;
  uint8_t excitedWeight;
  uint8_t idleSpeedPercent;
  uint8_t glanceAmplitude;
  uint16_t microSleepScalePercent;
};

struct PetExpression {
  float leftOpen = 1.0f;
  float rightOpen = 1.0f;
  float leftScaleX = 1.0f;
  float rightScaleX = 1.0f;
  int8_t leftX = 0;
  int8_t rightX = 0;
  int8_t leftY = 0;
  int8_t rightY = 0;
  int8_t leftBrowTilt = 0;
  int8_t rightBrowTilt = 0;
  int8_t leftBrowArch = 6;
  int8_t rightBrowArch = 6;
  int8_t browY = 0;
  bool hearts = false;
  bool sparkle = false;
  bool jitter = false;
  bool xEyes = false;
};

struct PetMotionInput {
  float tiltX = 0.0f;       // -1..1, screen left/right after calibration
  float tiltY = 0.0f;       // -1..1, screen up/down after calibration
  float motion = 0.0f;      // 0..1 overall movement intensity
  float motionX = 0.0f;     // -1..1 dynamic acceleration in screen coordinates
  float motionY = 0.0f;     // -1..1 dynamic acceleration in screen coordinates
  float angularZ = 0.0f;    // -1..1 rotation around the screen normal
  float rotationSpeed = 0.0f;
  float gravityZ = 1.0f;    // approx -1..1, useful for face-down/inverted reactions
  float inertiaStrength = 1.0f; // 0.5..1.5 user tuning
  float squashStrength = 1.0f;  // 0..1.5 user tuning
};

struct PetMotionPose {
  float shiftX = 0.0f;
  float shiftY = 0.0f;
  float squashX = 1.0f;
  float squashY = 1.0f;
  float cluster = 0.0f;
  float wobbleX = 0.0f;     // spring/inertia offset X
  float wobbleY = 0.0f;     // spring/inertia offset Y
  float cartoonTilt = 0.0f; // differential eye/brow tilt from rotational inertia
  float shakeAmount = 0.0f; // 0..1 used for subtle squash/stretch
  float sideAmount = 0.0f;
  float verticalAmount = 0.0f;
  bool faceDown = false;
};

constexpr uint8_t PET_SKIN_COUNT = 13;

const PetSkinDefinition& petSkinDefinition(uint8_t skinId);
const char* petSkinName(uint8_t skinId);
const char* petSkinDetail(uint8_t skinId);
const PetBehaviorProfile& petBehaviorProfile(uint8_t skinId);
PetExpression resolvePetExpression(PetMood mood, uint32_t now, float effectProgress);
PetMotionPose resolvePetMotion(const PetMotionInput& input, uint32_t now);
