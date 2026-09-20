#pragma once
#include <Arduino.h>
#include <math.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BMP085.h>

struct SensorSnapshot {
  bool mpuAvailable = false;
  bool bmpAvailable = false;

  float accelX = 0.0f;
  float accelY = 0.0f;
  float accelZ = 0.0f;
  float gyroX = 0.0f;
  float gyroY = 0.0f;
  float gyroZ = 0.0f;

  float gravityX = 0.0f; // screen-space normalized gravity, -1..1
  float gravityY = 0.0f;
  float gravityZ = 1.0f;
  float linearX = 0.0f;  // screen-space linear acceleration in m/s²
  float linearY = 0.0f;

  float temperatureC = NAN;
  float pressureHpa = NAN;

  float gazeX = 0.0f;   // calibrated stable tilt, -1..1
  float gazeY = 0.0f;
  float motion = 0.0f;  // smoothed activity 0..1
  float motionX = 0.0f; // directional dynamic acceleration, -1..1
  float motionY = 0.0f;
  uint32_t stillForMs = 0;
  bool restCalibrated = false;       // Session-only automatic neutral position.
  uint32_t restCalProgressMs = 0;    // 0..10000 ms of continuous stillness.
  bool freeFall = false;              // Sustained low-g state, useful for diagnostics.
};

class SensorHub {
 public:
  bool begin();
  void update(uint32_t now);
  const SensorSnapshot& snapshot() const { return data_; }

  bool consumePickup();
  bool consumeShake();
  bool consumeSettled();
  bool consumeFall();

  void setGazeCalibration(float offsetX, float offsetY);
  bool calibrateCurrent(float& offsetX, float& offsetY);
  void resetCalibration();
  void setMountRotation(uint8_t quarterTurns);

 private:
  bool beginMpu();
  bool beginBmp();
  void rotateScreen(float& x, float& y) const;

  Adafruit_MPU6050 mpu_;
  Adafruit_BMP085 bmp_;
  SensorSnapshot data_;

  uint32_t lastMpuAt_ = 0;
  uint32_t lastBmpAt_ = 0;
  uint32_t lastSampleAt_ = 0;

  uint32_t movingCandidateAt_ = 0;
  uint32_t stillCandidateAt_ = 0;
  uint32_t movingSince_ = 0;
  uint32_t stableStillSince_ = 0;
  bool movingState_ = false;
  bool pickupPending_ = false;
  bool shakePending_ = false;
  bool settledPending_ = false;
  bool shakeLatched_ = false;
  uint32_t shakeCandidateAt_ = 0;
  uint32_t freeFallCandidateAt_ = 0;
  uint32_t impactWindowUntil_ = 0;
  bool freeFallArmed_ = false;
  bool fallPending_ = false;

  float filteredGazeX_ = 0.0f;
  float filteredGazeY_ = 0.0f;
  float rawGazeX_ = 0.0f;
  float rawGazeY_ = 0.0f;
  float gazeOffsetX_ = 0.0f;
  float gazeOffsetY_ = 0.0f;
  float sessionRestOffsetX_ = 0.0f;
  float sessionRestOffsetY_ = 0.0f;
  bool autoRestCalibrated_ = false;
  uint32_t autoRestStillSince_ = 0;
  uint8_t mountRotation_ = 0;

  bool gravityInitialized_ = false;
  float gravityX_ = 0.0f;
  float gravityY_ = 0.0f;
  float gravityZ_ = 9.80665f;
  float filteredMotionX_ = 0.0f;
  float filteredMotionY_ = 0.0f;
  float filteredMotionLevel_ = 0.0f;
};
