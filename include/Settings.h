#pragma once
#include <Arduino.h>

struct Settings {
  uint16_t version = 11;

  uint16_t focusMinutes = 25;
  uint16_t breakMinutes = 5;
  uint16_t longBreakMinutes = 15;
  uint8_t sessionsBeforeLongBreak = 4;
  bool autoBreak = false;

  uint16_t faceMinSeconds = 4;
  uint16_t faceMaxSeconds = 12;
  uint8_t animationFps = 40;
  uint16_t longPressMs = 900;
  uint16_t doubleClickMs = 330;
  uint16_t petAnimationMs = 1600;
  uint8_t oledContrast = 180;
  uint16_t idleDimSeconds = 20; // 0 disables auto-dim; otherwise button/touch inactivity delay.
  bool screenFlipped = false;

  bool soundEnabled = true;
  uint8_t petSound = 0;
  uint8_t doneMelody = 0;
  uint8_t breakMelody = 0;

  uint8_t personality = 0;
  uint8_t accessoryMode = 255; // 255 = best unlocked accessory

  bool deskBuddyEnabled = true;
  bool touchReactionsEnabled = true;
  bool motionReactionsEnabled = true;
  bool environmentReactionsEnabled = true;
  bool advancedTouchEnabled = true;
  bool rareEventsEnabled = true;
  bool focusCompanionEnabled = true;
  bool speechBubblesEnabled = true;
  uint8_t speechEventChance = 60; // 0..100, contextual text chance. Boot greeting ignores this.
  uint16_t rareEventMinSeconds = 90;
  uint16_t rareEventMaxSeconds = 240;
  uint8_t idleAnimationSpeed = 100; // 50..150%
  uint8_t eyeFollowStrength = 100;  // 50..150%
  uint8_t inertiaStrength = 100;    // 50..150%
  uint8_t squashStrength = 100;     // 0..150%
  uint8_t heartParticleCount = 10;  // 0..16
  bool swapTouchButtons = false;
  float mpuGazeOffsetX = 0.0f;
  float mpuGazeOffsetY = 0.0f;
  uint8_t mpuMountRotation = 0; // 0,1,2,3 = 0/90/180/270 degrees in screen plane.
  uint8_t motionSensitivity = 100; // 50..150%, affects cartoon inertial movement only.

  bool sleepEnabled = false;
  uint8_t sleepStartHour = 23;
  uint8_t sleepEndHour = 7;

  char petName[16] = "PIXEL"; // Metadata only, not rendered on the OLED.
};
