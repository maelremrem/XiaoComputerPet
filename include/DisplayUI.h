#pragma once
#include <Arduino.h>
#include "ProjectConfig.h"
#include "PetState.h"
#include "Settings.h"
#include "PetVisual.h"

#include <Adafruit_GFX.h>
#if OLED_USE_SH1106
#include <Adafruit_SH110X.h>
#define OLED_WHITE SH110X_WHITE
#else
#include <Adafruit_SSD1306.h>
#define OLED_WHITE SSD1306_WHITE
#endif

enum class PetMenuView : uint8_t {
  Root,
  Stats,
  Accessory,
  Personality,
  Options,
  DeskBuddy,
  Sound,
  Sleep,
  PetSound,
  DoneMelody,
  BreakMelody,
  ScreenRotation,
  TouchLayout,
  MpuCalibration
};

class DisplayUI {
 public:
  DisplayUI();
  bool begin(uint8_t contrast);
  void setContrast(uint8_t contrast);
  void setFlipped(bool flipped);
  void setAnimationTuning(uint8_t idleSpeed, uint8_t heartParticles, bool skinPersonalityEnabled = true);
  void renderPet(PetMood mood, uint32_t now, float effectProgress, uint8_t personality, uint8_t accessory,
                 float pressAmount = 0.0f, const PetMotionInput& motion = PetMotionInput{},
                 const char* speechText = nullptr, float speechProgress = 0.0f, bool sleeping = false);
  void renderPomodoroReady(uint8_t timerMode, uint8_t previousTimerMode,
                            uint16_t minutes, uint16_t previousMinutes,
                            float modeTransition, int8_t modeDirection,
                            float temperatureC, float pressureHpa, bool ambientAvailable, uint32_t now);
  void renderPomodoro(uint32_t remainingMs, uint32_t totalMs, bool paused, uint8_t timerMode,
                       float temperatureC, float pressureHpa, bool ambientAvailable,
                       bool focusCompanion, uint8_t timerCompanionLayout, uint8_t personality, uint32_t now);
  void renderStats(const PetState& state);
  void renderPetMenu(PetMenuView view, uint8_t index, uint8_t previousIndex, float transition,
                     int8_t direction, uint32_t now, const PetState& state,
                     uint8_t highestUnlockedAccessory, float holdProgress);
  void renderConfigHint();
  uint32_t framesPresented() const { return framesPresented_; }
  uint32_t framesSkipped() const { return framesSkipped_; }

 private:
#if OLED_USE_SH1106
  Adafruit_SH1106G display_;
#else
  Adafruit_SSD1306 display_;
#endif
  void drawFace(PetMood mood, uint32_t now, float effectProgress, uint8_t personality, float pressAmount, const PetMotionInput& motion);
  void drawStyledEye(PetEyeStyle style, int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t radius, bool leftEye, uint32_t now);
  void drawStyledBrow(PetBrowStyle style, int16_t cx, int16_t cy, int16_t halfWidth, int8_t tilt, int8_t arch, uint8_t thickness);
  void drawAccessory(uint8_t accessory, uint32_t now);
  void drawEye(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t radius);
  void drawBrowArc(int16_t cx, int16_t cy, int16_t halfWidth, int8_t tilt, int8_t arch, uint8_t thickness);
  void drawHeartEye(int16_t cx, int16_t cy, uint8_t size);
  void drawXEye(int16_t cx, int16_t cy, int16_t w, int16_t h, uint8_t thickness);
  void drawHeartParticles(uint32_t now, float envelope);
  void drawSpeechPet(PetMood mood, uint32_t now, uint8_t personality, const PetMotionInput& motion, float zoomProgress);
  void drawSpeechBubble(const char* text, float progress);
  void drawSleepZzz(uint32_t now);
  void drawFocusCompanion(float progress, bool paused, uint8_t timerMode, uint32_t remainingMs, uint32_t now);
  void drawPomodoroPet(float progress, bool paused, uint8_t timerMode, uint8_t personality, uint8_t layout, uint32_t now);
  void presentIfChanged(bool force = false);
  void drawTimerLayer(const char* text, int16_t y, uint8_t alphaStep, int16_t xOffset = 0);
  void drawSmallTextLayer(const char* text, int16_t x, int16_t y, uint8_t alphaStep);
  void drawTimerGlyph(char glyph, int16_t x, int16_t y, uint8_t alphaStep, uint8_t textSize = 2);
  void drawSparkle(int16_t x, int16_t y, uint8_t size);
  void centerText(const char* text, int16_t y, uint8_t size);

  void drawMenuHeader(const char* label);
  void drawMenuFooter(uint8_t index, uint8_t count);
  void drawMenuIcon(uint8_t icon, int16_t cx, int16_t cy, uint32_t now);
  void drawMenuCard(int16_t x, const char* title, const char* detail, uint8_t icon, uint32_t now, bool active);
  void drawStatsCard(int16_t x, uint8_t page, const PetState& state, uint32_t now, bool active);
  void drawHoldProgress(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, float progress);
  float easeOutCubic(float t);

  uint32_t timerShownSeconds_ = 0;
  uint32_t timerFromSeconds_ = 0;
  uint32_t timerToSeconds_ = 0;
  uint32_t timerTransitionAt_ = 0;
  bool timerInitialized_ = false;
  uint8_t idleAnimationSpeed_ = 100;
  uint8_t heartParticleCount_ = 10;
  bool skinPersonalityEnabled_ = true;
  uint32_t lastFrameHash_ = 0;
  bool frameHashValid_ = false;
  uint32_t framesPresented_ = 0;
  uint32_t framesSkipped_ = 0;
};
