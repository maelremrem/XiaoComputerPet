#include "PetVisual.h"
#include "ProjectConfig.h"
#include <math.h>

static float clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

static float eventEnvelope(float p) {
  const float attack = clamp01(p / 0.14f);
  const float release = p > 0.80f ? clamp01((1.0f - p) / 0.20f) : 1.0f;
  const float a = attack * attack * (3.0f - 2.0f * attack);
  const float r = release * release * (3.0f - 2.0f * release);
  return a < r ? a : r;
}

static bool usesEventEnvelope(PetMood mood) {
  switch (mood) {
    case PetMood::Yawn:
    case PetMood::Sneeze:
    case PetMood::Dance:
    case PetMood::Dream:
    case PetMood::Hiccup:
    case PetMood::ScratchLeft:
    case PetMood::ScratchRight:
    case PetMood::Hug:
    case PetMood::Dizzy:
      return true;
    default:
      return false;
  }
}

static void blendTowardIdle(PetExpression& e, float amount) {
  const float keep = clamp01(amount);
  e.leftOpen = 1.0f + (e.leftOpen - 1.0f) * keep;
  e.rightOpen = 1.0f + (e.rightOpen - 1.0f) * keep;
  e.leftScaleX = 1.0f + (e.leftScaleX - 1.0f) * keep;
  e.rightScaleX = 1.0f + (e.rightScaleX - 1.0f) * keep;
  e.leftX = static_cast<int8_t>(roundf(e.leftX * keep));
  e.rightX = static_cast<int8_t>(roundf(e.rightX * keep));
  e.leftY = static_cast<int8_t>(roundf(e.leftY * keep));
  e.rightY = static_cast<int8_t>(roundf(e.rightY * keep));
  e.leftBrowTilt = static_cast<int8_t>(roundf(e.leftBrowTilt * keep));
  e.rightBrowTilt = static_cast<int8_t>(roundf(e.rightBrowTilt * keep));
  e.leftBrowArch = static_cast<int8_t>(roundf(6.0f + (e.leftBrowArch - 6.0f) * keep));
  e.rightBrowArch = static_cast<int8_t>(roundf(6.0f + (e.rightBrowArch - 6.0f) * keep));
  e.browY = static_cast<int8_t>(roundf(e.browY * keep));
  if (keep < 0.15f) {
    e.sparkle = false;
    e.hearts = false;
  }
}

static const PetSkinDefinition SKINS[PET_SKIN_COUNT] = {
  {"SOFT",     "rounded",     39, 89, 38, 25, 26, 7, PetEyeStyle::Rounded, PetBrowStyle::Arc,      6, 0},
  {"ROBOT",    "angular",     39, 89, 38, 26, 24, 3, PetEyeStyle::Square,  PetBrowStyle::Flat,     7, 0},
  {"COMPACT",  "close set",   43, 85, 38, 21, 28, 6, PetEyeStyle::Rounded, PetBrowStyle::Arc,      6, 0},
  {"WIDE",     "wide set",    37, 91, 38, 30, 21, 7, PetEyeStyle::Pill,    PetBrowStyle::Arc,      6, 0},
  {"ARCADE",   "8-bit",       39, 89, 38, 25, 25, 0, PetEyeStyle::Pixel,   PetBrowStyle::Short,    5, 0},
  {"ALIEN",    "slanted",     39, 89, 38, 31, 18, 5, PetEyeStyle::Alien,   PetBrowStyle::Floating, 5, 0},
  {"CAT",      "feline",      39, 89, 39, 29, 23, 4, PetEyeStyle::Cat,     PetBrowStyle::Arc,      6, 0},
  {"VISOR",    "linked",      40, 88, 38, 34, 18, 8, PetEyeStyle::Visor,   PetBrowStyle::Flat,     5, 1},
  {"CORE",     "orbital",     40, 88, 38, 25, 25, 9, PetEyeStyle::Ring,    PetBrowStyle::Floating, 5, 0},
  {"SLEEPBOT", "low profile", 39, 89, 40, 31, 15, 7, PetEyeStyle::Sleep,   PetBrowStyle::Arc,      7, 0},
  {"SCOUT",    "binocular",   38, 90, 38, 27, 27, 9, PetEyeStyle::Scout,   PetBrowStyle::Short,    5, 0},
  {"BUBBLE",   "big eyes",    40, 88, 38, 30, 30,12, PetEyeStyle::Bubble,  PetBrowStyle::Floating, 5, 0},
  {"MASK",     "sharp",       39, 89, 38, 31, 22, 3, PetEyeStyle::Mask,    PetBrowStyle::Flat,     7, 1},
};

static const PetBehaviorProfile PROFILES[PET_SKIN_COUNT] = {
  {18,18,18,16,14,16, 95,2,110}, // Soft
  {12,18,10,28,10,22,115,2,125}, // Robot
  {22,14,20,14,18,12, 90,1, 90}, // Compact
  {14,16,18,16,14,22,105,3,110}, // Wide
  {10,14,18,14, 8,36,135,3,145}, // Arcade
  {12,34,10,16,10,18,110,4,120}, // Alien
  {16,30,18,10,16,10,100,4, 95}, // Cat
  {10,18, 8,34,12,18,110,2,135}, // Visor
  {14,24,12,20,12,18,100,3,115}, // Core
  {24,10,10,10,38, 8, 75,1, 65}, // Sleepbot
  {12,26,10,30,10,12,105,3,125}, // Scout
  {18,18,30, 8,16,10, 90,2, 90}, // Bubble
  {10,18,10,32,10,20,120,2,140}, // Mask
};

const PetSkinDefinition& petSkinDefinition(uint8_t skinId) {
  return SKINS[skinId % PET_SKIN_COUNT];
}

const char* petSkinName(uint8_t skinId) {
  return petSkinDefinition(skinId).name;
}

const char* petSkinDetail(uint8_t skinId) {
  return petSkinDefinition(skinId).detail;
}

const PetBehaviorProfile& petBehaviorProfile(uint8_t skinId) {
  return PROFILES[skinId % PET_SKIN_COUNT];
}

PetExpression resolvePetExpression(PetMood mood, uint32_t now, float p) {
  PetExpression e;
  switch (mood) {
    case PetMood::Blink:
      e.leftOpen = e.rightOpen = 0.14f;
      break;
    case PetMood::Curious:
      e.rightOpen = 0.68f;
      e.rightY = 4;
      e.leftBrowTilt = -3;
      e.rightBrowTilt = 3;
      e.rightBrowArch = 9;
      break;
    case PetMood::Sleepy:
      e.leftOpen = e.rightOpen = 0.18f;
      e.leftY = e.rightY = 4;
      e.browY = 6;
      e.leftBrowArch = e.rightBrowArch = 2;
      break;
    case PetMood::Happy:
      e.leftOpen = e.rightOpen = 0.38f;
      e.leftY = e.rightY = 3;
      e.leftBrowTilt = -2;
      e.rightBrowTilt = 2;
      e.leftBrowArch = e.rightBrowArch = 8;
      break;
    case PetMood::Affectionate:
      e.hearts = true;
      e.sparkle = true;
      break;
    case PetMood::Excited: {
      const int8_t b = ((now / 95) & 1) ? -2 : 2;
      e.leftY = b;
      e.rightY = -b;
      e.leftScaleX = e.rightScaleX = 1.12f;
      e.leftOpen = e.rightOpen = 1.08f;
      e.leftBrowArch = e.rightBrowArch = 10;
      break;
    }
    case PetMood::Done:
      e.leftOpen = e.rightOpen = 0.40f;
      e.leftY = e.rightY = 2;
      e.leftBrowArch = e.rightBrowArch = 10;
      e.sparkle = true;
      break;
    case PetMood::Surprised:
      e.leftScaleX = e.rightScaleX = 1.16f;
      e.leftOpen = e.rightOpen = 1.18f;
      e.browY = -5;
      e.leftBrowArch = e.rightBrowArch = 3;
      break;
    case PetMood::Focused:
      e.leftOpen = e.rightOpen = 0.58f;
      e.leftBrowTilt = 4;
      e.rightBrowTilt = -4;
      e.leftBrowArch = e.rightBrowArch = 3;
      break;
    case PetMood::Bored:
      e.leftOpen = e.rightOpen = 0.40f;
      e.leftY = e.rightY = 4;
      e.browY = 6;
      e.leftBrowArch = e.rightBrowArch = 1;
      break;
    case PetMood::Yawn: {
      const float phase = 0.5f + 0.5f * sinf(static_cast<float>(now) * 0.0045f);
      e.leftOpen = e.rightOpen = 0.10f + 0.28f * phase;
      e.leftY = e.rightY = 4;
      e.browY = 5;
      e.leftBrowArch = e.rightBrowArch = 2;
      break;
    }
    case PetMood::Sneeze: {
      const uint16_t phase = now % 720UL;
      if (phase < 250) {
        e.leftOpen = e.rightOpen = 0.18f;
        e.leftScaleX = e.rightScaleX = 1.08f;
        e.browY = 4;
      } else if (phase < 360) {
        e.leftOpen = e.rightOpen = 0.08f;
        e.leftX = -3; e.rightX = 3;
        e.leftBrowTilt = 5; e.rightBrowTilt = -5;
      } else {
        e.leftOpen = e.rightOpen = 0.75f;
        e.leftScaleX = e.rightScaleX = 1.08f;
      }
      break;
    }
    case PetMood::Dance: {
      const float wave = sinf(static_cast<float>(now) * 0.015f);
      e.leftY = static_cast<int8_t>(wave * 3.0f);
      e.rightY = static_cast<int8_t>(-wave * 3.0f);
      e.leftX = static_cast<int8_t>(wave * 2.0f);
      e.rightX = static_cast<int8_t>(wave * 2.0f);
      e.leftOpen = e.rightOpen = 0.82f;
      e.leftBrowTilt = static_cast<int8_t>(wave * 4.0f);
      e.rightBrowTilt = e.leftBrowTilt;
      e.sparkle = true;
      break;
    }
    case PetMood::Dream:
      e.leftOpen = e.rightOpen = 0.12f;
      e.leftY = e.rightY = 5;
      e.browY = 5;
      e.leftBrowArch = e.rightBrowArch = 2;
      e.sparkle = true;
      break;
    case PetMood::Hiccup: {
      const bool hop = (now % 900UL) < 130UL;
      e.leftY = e.rightY = hop ? -5 : 1;
      e.leftOpen = e.rightOpen = hop ? 1.15f : 0.72f;
      e.browY = hop ? -3 : 0;
      break;
    }
    case PetMood::ScratchLeft:
      e.leftOpen = 0.20f; e.rightOpen = 0.72f;
      e.leftY = 3; e.leftBrowTilt = -4; e.rightBrowTilt = 2;
      break;
    case PetMood::ScratchRight:
      e.rightOpen = 0.20f; e.leftOpen = 0.72f;
      e.rightY = 3; e.rightBrowTilt = 4; e.leftBrowTilt = -2;
      break;
    case PetMood::Hug:
      e.hearts = true;
      e.sparkle = true;
      e.leftOpen = e.rightOpen = 0.45f;
      e.leftBrowArch = e.rightBrowArch = 10;
      break;
    case PetMood::Dizzy:
      // Motion itself is handled by the spring/inertia layer. Keep Dizzy as
      // an expression only so it never fights the physical head movement.
      e.leftScaleX = e.rightScaleX = 0.88f;
      e.leftOpen = e.rightOpen = 0.72f;
      e.leftBrowTilt = 5;
      e.rightBrowTilt = -5;
      e.leftBrowArch = e.rightBrowArch = 4;
      break;
    case PetMood::Knocked:
      e.xEyes = true;
      e.leftBrowTilt = 5;
      e.rightBrowTilt = -5;
      e.leftBrowArch = e.rightBrowArch = 2;
      e.browY = 2;
      break;
    default:
      break;
  }

  if (usesEventEnvelope(mood)) {
    blendTowardIdle(e, eventEnvelope(p));
  }

  if (e.hearts) {
    const float intro = clamp01(p * 6.5f);
    const float outro = p > 0.78f ? clamp01((1.0f - p) / 0.22f) : 1.0f;
    const float env = intro < outro ? intro : outro;
    e.leftScaleX = e.rightScaleX = 0.55f + env * 0.70f;
  }
  return e;
}

PetMotionPose resolvePetMotion(const PetMotionInput& in, uint32_t now) {
  PetMotionPose p;

  auto smoothstep = [](float edge0, float edge1, float x) {
    if (edge1 <= edge0) return x >= edge1 ? 1.0f : 0.0f;
    x = constrain((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
  };

  const float tx = constrain(in.tiltX, -1.0f, 1.0f);
  const float ty = constrain(in.tiltY, -1.0f, 1.0f);
  const float ax = fabsf(tx);
  const float ay = fabsf(ty);

  // Static gravity is intentionally slow and separate from shake physics.
  // When the pet rests on a side, the face falls toward that edge and compresses.
  p.sideAmount = smoothstep(0.18f, 0.72f, ax);
  p.verticalAmount = smoothstep(0.20f, 0.76f, ay);
  p.shiftX = tx * (3.0f + 7.5f * p.sideAmount);
  p.shiftY = ty * (2.4f + 5.2f * p.verticalAmount);
  p.cluster = p.sideAmount * 5.5f;
  p.squashX = 1.0f - 0.20f * p.sideAmount;
  p.squashY = 1.0f - 0.12f * p.verticalAmount;

  // One physical model for the whole head. Dynamic acceleration acts as a
  // force; the spring only wants to return to center. This produces lag,
  // overshoot and a natural settle without deriving noisy jerk twice.
  struct SpringState {
    bool initialized = false;
    uint32_t lastAt = 0;
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float angle = 0.0f;
    float angularV = 0.0f;
  };
  static SpringState spring;

  float dt = 1.0f / 40.0f;
  if (!spring.initialized) {
    spring.initialized = true;
    spring.lastAt = now;
  } else {
    const uint32_t elapsedMs = now - spring.lastAt;
    spring.lastAt = now;
    if (elapsedMs > 140) {
      // Pet rendering was interrupted by a menu/timer. Never resume an old
      // wobble when returning to the face.
      spring.x = spring.y = spring.vx = spring.vy = 0.0f;
      spring.angle = spring.angularV = 0.0f;
    } else {
      dt = constrain(static_cast<float>(elapsedMs) * 0.001f, 0.008f, 0.040f);
    }
  }

  const float movement = clamp01(in.motion);
  float mx = constrain(in.motionX, -1.0f, 1.0f);
  float my = constrain(in.motionY, -1.0f, 1.0f);
  float rz = constrain(in.angularZ, -1.0f, 1.0f);
  if (fabsf(mx) < 0.020f) mx = 0.0f;
  if (fabsf(my) < 0.020f) my = 0.0f;
  if (fabsf(rz) < 0.030f) rz = 0.0f;

  const float damping = movement > 0.10f
    ? hw::PET_SPRING_DAMPING_MOVING
    : hw::PET_SPRING_DAMPING_REST;
  const float k = hw::PET_SPRING_STIFFNESS;

  // The minus sign is the cartoon inertia: move the device right and the face
  // initially lags left, then catches up and overshoots when the hand stops.
  const float inertia = constrain(in.inertiaStrength, 0.5f, 1.5f);
  const float forceX = -mx * hw::PET_MOTION_FORCE_X * inertia;
  const float forceY = -my * hw::PET_MOTION_FORCE_Y * inertia;
  const float forceA = ((-rz * hw::PET_ROTATION_FORCE) + (-mx * 18.0f)) * inertia;

  const float accelX = forceX - k * spring.x - damping * spring.vx;
  const float accelY = forceY - k * spring.y - damping * spring.vy;
  const float angularAccel = forceA - (k * 0.75f) * spring.angle - (damping * 0.86f) * spring.angularV;

  spring.vx += accelX * dt;
  spring.vy += accelY * dt;
  spring.angularV += angularAccel * dt;
  spring.x += spring.vx * dt;
  spring.y += spring.vy * dt;
  spring.angle += spring.angularV * dt;

  // Clamp only at extreme travel. Normal motion never touches these limits, so
  // the spring remains physically consistent instead of being re-shaped each frame.
  if (spring.x < -hw::PET_MAX_WOBBLE_X) { spring.x = -hw::PET_MAX_WOBBLE_X; if (spring.vx < 0.0f) spring.vx *= -0.12f; }
  if (spring.x >  hw::PET_MAX_WOBBLE_X) { spring.x =  hw::PET_MAX_WOBBLE_X; if (spring.vx > 0.0f) spring.vx *= -0.12f; }
  if (spring.y < -hw::PET_MAX_WOBBLE_Y) { spring.y = -hw::PET_MAX_WOBBLE_Y; if (spring.vy < 0.0f) spring.vy *= -0.12f; }
  if (spring.y >  hw::PET_MAX_WOBBLE_Y) { spring.y =  hw::PET_MAX_WOBBLE_Y; if (spring.vy > 0.0f) spring.vy *= -0.12f; }
  if (spring.angle < -hw::PET_MAX_ROTATION) { spring.angle = -hw::PET_MAX_ROTATION; if (spring.angularV < 0.0f) spring.angularV *= -0.10f; }
  if (spring.angle >  hw::PET_MAX_ROTATION) { spring.angle =  hw::PET_MAX_ROTATION; if (spring.angularV > 0.0f) spring.angularV *= -0.10f; }

  if (movement < 0.025f) {
    if (fabsf(spring.x) < 0.025f && fabsf(spring.vx) < 0.10f) spring.x = spring.vx = 0.0f;
    if (fabsf(spring.y) < 0.025f && fabsf(spring.vy) < 0.10f) spring.y = spring.vy = 0.0f;
    if (fabsf(spring.angle) < 0.025f && fabsf(spring.angularV) < 0.08f) spring.angle = spring.angularV = 0.0f;
  }

  p.wobbleX = spring.x;
  p.wobbleY = spring.y;
  p.cartoonTilt = spring.angle + constrain(spring.vx * 0.028f, -1.4f, 1.4f);

  const float speed = sqrtf(spring.vx * spring.vx + spring.vy * spring.vy);
  p.shakeAmount = clamp01(movement * 0.60f + speed * 0.018f + fabsf(spring.angularV) * 0.020f);

  // A small squash/stretch follows velocity, not raw acceleration. That makes
  // it peak just after the directional movement like traditional animation.
  const float squash = p.shakeAmount * 0.085f * constrain(in.squashStrength, 0.0f, 1.5f);
  if (fabsf(spring.vx) > fabsf(spring.vy)) {
    p.squashX *= 1.0f + squash;
    p.squashY *= 1.0f - squash * 0.55f;
  } else {
    p.squashY *= 1.0f + squash * 0.72f;
    p.squashX *= 1.0f - squash * 0.42f;
  }

  p.faceDown = in.gravityZ < -0.45f;
  return p;
}
