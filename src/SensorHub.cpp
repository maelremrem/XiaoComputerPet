#include "SensorHub.h"
#include "ProjectConfig.h"
#include <Wire.h>
#include <math.h>

namespace {
constexpr float GRAVITY = 9.80665f;
constexpr uint32_t AUTO_REST_CAL_MS = 10000;

float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

float deadband(float v, float threshold) {
  const float a = fabsf(v);
  if (a <= threshold) return 0.0f;
  const float sign = v < 0.0f ? -1.0f : 1.0f;
  return sign * (a - threshold);
}

float smoothingAlpha(float dt, float tau) {
  if (tau <= 0.0f) return 1.0f;
  return clampf(dt / (tau + dt), 0.0f, 1.0f);
}
}

bool SensorHub::beginMpu() {
  if (mpu_.begin(hw::MPU6050_ADDRESS_PRIMARY, &Wire)) return true;
  return mpu_.begin(hw::MPU6050_ADDRESS_SECONDARY, &Wire);
}

bool SensorHub::beginBmp() {
  return bmp_.begin(BMP085_STANDARD);
}

bool SensorHub::begin() {
  data_.mpuAvailable = beginMpu();
  if (data_.mpuAvailable) {
    mpu_.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu_.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu_.setFilterBandwidth(MPU6050_BAND_44_HZ);
  }

  data_.bmpAvailable = beginBmp();
  stableStillSince_ = millis();
  data_.stillForMs = 0;
  autoRestCalibrated_ = false;
  autoRestStillSince_ = 0;
  sessionRestOffsetX_ = 0.0f;
  sessionRestOffsetY_ = 0.0f;
  data_.restCalibrated = false;
  data_.restCalProgressMs = 0;
  return data_.mpuAvailable || data_.bmpAvailable;
}

void SensorHub::setMountRotation(uint8_t quarterTurns) {
  mountRotation_ = quarterTurns & 0x03;
  filteredGazeX_ = 0.0f;
  filteredGazeY_ = 0.0f;
  filteredMotionX_ = 0.0f;
  filteredMotionY_ = 0.0f;
  sessionRestOffsetX_ = 0.0f;
  sessionRestOffsetY_ = 0.0f;
  autoRestCalibrated_ = false;
  autoRestStillSince_ = 0;
  data_.restCalibrated = false;
  data_.restCalProgressMs = 0;
}

void SensorHub::rotateScreen(float& x, float& y) const {
  const float ox = x;
  const float oy = y;
  switch (mountRotation_ & 0x03) {
    case 1: x = -oy; y = ox; break;
    case 2: x = -ox; y = -oy; break;
    case 3: x = oy; y = -ox; break;
    default: break;
  }
}

void SensorHub::update(uint32_t now) {
  if (data_.mpuAvailable && (now - lastMpuAt_) >= 10) {
    lastMpuAt_ = now;

    float dt = 0.01f;
    if (lastSampleAt_ != 0) {
      dt = static_cast<float>(now - lastSampleAt_) * 0.001f;
      dt = clampf(dt, 0.004f, 0.05f);
    }
    lastSampleAt_ = now;

    sensors_event_t a, g, temp;
    mpu_.getEvent(&a, &g, &temp);
    data_.accelX = a.acceleration.x;
    data_.accelY = a.acceleration.y;
    data_.accelZ = a.acceleration.z;
    data_.gyroX = g.gyro.x;
    data_.gyroY = g.gyro.y;
    data_.gyroZ = g.gyro.z;

    const float rawNorm = sqrtf(
      a.acceleration.x * a.acceleration.x +
      a.acceleration.y * a.acceleration.y +
      a.acceleration.z * a.acceleration.z
    );
    const float gyroSpeed = sqrtf(
      g.gyro.x * g.gyro.x +
      g.gyro.y * g.gyro.y +
      g.gyro.z * g.gyro.z
    );

    // Drop detection is deliberately independent from the normal motion score.
    // A real drop has a low-g/free-fall phase; shaking the enclosure usually
    // keeps the accelerometer magnitude near 1 g and therefore won't trigger it.
    constexpr float FREE_FALL_NORM = GRAVITY * 0.33f;
    constexpr uint32_t FREE_FALL_MIN_MS = 70;
    constexpr float IMPACT_NORM = GRAVITY * 1.75f;
    constexpr uint32_t IMPACT_WINDOW_MS = 850;

    const bool lowG = rawNorm < FREE_FALL_NORM;
    if (lowG) {
      if (freeFallCandidateAt_ == 0) freeFallCandidateAt_ = now;
      if (!freeFallArmed_ && (now - freeFallCandidateAt_) >= FREE_FALL_MIN_MS) {
        freeFallArmed_ = true;
        fallPending_ = true; // Show X eyes immediately while falling.
      }
      data_.freeFall = freeFallArmed_;
    } else {
      freeFallCandidateAt_ = 0;
      data_.freeFall = false;
      if (freeFallArmed_ && impactWindowUntil_ == 0) {
        impactWindowUntil_ = now + IMPACT_WINDOW_MS;
      }
      if (freeFallArmed_ && rawNorm >= IMPACT_NORM) {
        // Re-trigger on impact so the knocked-out face remains visible after landing.
        fallPending_ = true;
        freeFallArmed_ = false;
        impactWindowUntil_ = 0;
      } else if (freeFallArmed_ && impactWindowUntil_ != 0 &&
                 static_cast<int32_t>(now - impactWindowUntil_) >= 0) {
        freeFallArmed_ = false;
        impactWindowUntil_ = 0;
      }
    }

    // Gravity is estimated separately from movement. During strong acceleration
    // or rotation we trust the accelerometer much less, so a shake no longer
    // looks like a sudden tilt of the whole pet.
    if (!gravityInitialized_) {
      gravityX_ = a.acceleration.x;
      gravityY_ = a.acceleration.y;
      gravityZ_ = a.acceleration.z;
      gravityInitialized_ = true;
    } else {
      const float normError = fabsf(rawNorm - GRAVITY);
      const float accelTrust = 1.0f - clampf(normError / 3.2f, 0.0f, 1.0f);
      const float gyroTrust = 1.0f - clampf((gyroSpeed - 0.45f) / 4.0f, 0.0f, 1.0f);
      const float trust = accelTrust * gyroTrust;
      const float alpha = smoothingAlpha(dt, 0.12f) * (0.12f + 0.88f * trust);
      gravityX_ += (a.acceleration.x - gravityX_) * alpha;
      gravityY_ += (a.acceleration.y - gravityY_) * alpha;
      gravityZ_ += (a.acceleration.z - gravityZ_) * alpha;

      // Keep the gravity estimate at 1 g. This prevents its magnitude from
      // slowly absorbing movement energy and corrupting linear acceleration.
      const float gMag = sqrtf(gravityX_ * gravityX_ + gravityY_ * gravityY_ + gravityZ_ * gravityZ_);
      if (gMag > 1.0f) {
        const float scale = GRAVITY / gMag;
        gravityX_ *= scale;
        gravityY_ *= scale;
        gravityZ_ *= scale;
      }
    }

    float screenGX = (gravityX_ / GRAVITY) * hw::MPU_GAZE_X_SIGN;
    float screenGY = (gravityY_ / GRAVITY) * hw::MPU_GAZE_Y_SIGN;
    if (hw::MPU_SWAP_GAZE_AXES) {
      const float t = screenGX;
      screenGX = screenGY;
      screenGY = t;
    }
    rotateScreen(screenGX, screenGY);

    // Gravity components are a stable, intuitive tilt signal. They remain calm
    // while the unit is being shaken because the gravity estimator is gated.
    rawGazeX_ = clampf(screenGX / 0.82f, -1.25f, 1.25f);
    rawGazeY_ = clampf(screenGY / 0.82f, -1.25f, 1.25f);
    float gazeX = clampf(rawGazeX_ - gazeOffsetX_ - sessionRestOffsetX_, -1.0f, 1.0f);
    float gazeY = clampf(rawGazeY_ - gazeOffsetY_ - sessionRestOffsetY_, -1.0f, 1.0f);
    const float gazeAlpha = smoothingAlpha(dt, 0.035f);
    filteredGazeX_ += (gazeX - filteredGazeX_) * gazeAlpha;
    filteredGazeY_ += (gazeY - filteredGazeY_) * gazeAlpha;
    if (fabsf(filteredGazeX_) < 0.015f) filteredGazeX_ = 0.0f;
    if (fabsf(filteredGazeY_) < 0.015f) filteredGazeY_ = 0.0f;
    // Do not expose a static tilt before the boot rest-zero has been learned.
    // Dynamic motion still works, but the face stays visually centered instead
    // of leaning on a sensor/mounting bias during the 10 s learning window.
    data_.gazeX = autoRestCalibrated_ ? filteredGazeX_ : 0.0f;
    data_.gazeY = autoRestCalibrated_ ? filteredGazeY_ : 0.0f;

    // Linear acceleration = accelerometer - stable gravity estimate.
    float linearX = (a.acceleration.x - gravityX_) * hw::MPU_MOTION_X_SIGN;
    float linearY = (a.acceleration.y - gravityY_) * hw::MPU_MOTION_Y_SIGN;
    if (hw::MPU_SWAP_MOTION_AXES) {
      const float t = linearX;
      linearX = linearY;
      linearY = t;
    }
    rotateScreen(linearX, linearY);

    const float linXFiltered = deadband(linearX, 0.16f);
    const float linYFiltered = deadband(linearY, 0.16f);
    const float motionAlpha = smoothingAlpha(dt, 0.035f);
    filteredMotionX_ += (linXFiltered - filteredMotionX_) * motionAlpha;
    filteredMotionY_ += (linYFiltered - filteredMotionY_) * motionAlpha;

    data_.linearX = filteredMotionX_;
    data_.linearY = filteredMotionY_;
    data_.motionX = clampf(filteredMotionX_ / 4.5f, -1.0f, 1.0f);
    data_.motionY = clampf(filteredMotionY_ / 4.5f, -1.0f, 1.0f);

    const float linearZ = deadband(a.acceleration.z - gravityZ_, 0.16f);
    const float linearMag = sqrtf(
      filteredMotionX_ * filteredMotionX_ +
      filteredMotionY_ * filteredMotionY_ +
      linearZ * linearZ
    );
    const float motionTarget = clampf(linearMag / 5.5f + gyroSpeed / 7.0f, 0.0f, 1.0f);
    const float levelTau = motionTarget > filteredMotionLevel_ ? 0.045f : 0.22f;
    filteredMotionLevel_ += (motionTarget - filteredMotionLevel_) * smoothingAlpha(dt, levelTau);
    if (filteredMotionLevel_ < 0.018f) filteredMotionLevel_ = 0.0f;
    data_.motion = filteredMotionLevel_;

    data_.gravityX = clampf(screenGX, -1.0f, 1.0f);
    data_.gravityY = clampf(screenGY, -1.0f, 1.0f);
    data_.gravityZ = clampf(gravityZ_ / GRAVITY, -1.0f, 1.0f);

    // Event detection has hysteresis and persistence. Small desk vibrations no
    // longer spam pickup/settled/shake reactions.
    const bool instantMoving = filteredMotionLevel_ > 0.13f || gyroSpeed > 0.75f;
    if (instantMoving) {
      stillCandidateAt_ = 0;
      if (!movingState_) {
        if (movingCandidateAt_ == 0) movingCandidateAt_ = now;
        if ((now - movingCandidateAt_) >= 110) {
          movingState_ = true;
          movingSince_ = movingCandidateAt_;
          if ((movingCandidateAt_ - stableStillSince_) > 1800) pickupPending_ = true;
        }
      }
    } else {
      movingCandidateAt_ = 0;
      if (movingState_) {
        if (stillCandidateAt_ == 0) stillCandidateAt_ = now;
        if ((now - stillCandidateAt_) >= 320) {
          const uint32_t movementDuration = stillCandidateAt_ - movingSince_;
          movingState_ = false;
          stableStillSince_ = stillCandidateAt_;
          if (movementDuration >= 280) settledPending_ = true;
          stillCandidateAt_ = 0;
        }
      }
    }

    if (!movingState_) data_.stillForMs = now - stableStillSince_;
    else data_.stillForMs = 0;

    // Automatic session-only rest calibration. The counter advances only while
    // the unit is genuinely quiet. Any movement resets the full 10 s window.
    // We keep this offset separate from the user calibration stored in flash so
    // every boot can adapt to the exact way the pet is resting on the desk.
    if (!autoRestCalibrated_) {
      const bool calibrationStill = !movingState_ &&
        filteredMotionLevel_ < 0.045f &&
        gyroSpeed < 0.18f &&
        linearMag < 0.45f;

      if (calibrationStill) {
        if (autoRestStillSince_ == 0) autoRestStillSince_ = now;
        const uint32_t elapsed = now - autoRestStillSince_;
        data_.restCalProgressMs = elapsed > AUTO_REST_CAL_MS ? AUTO_REST_CAL_MS : elapsed;
        if (elapsed >= AUTO_REST_CAL_MS) {
          sessionRestOffsetX_ = rawGazeX_ - gazeOffsetX_;
          sessionRestOffsetY_ = rawGazeY_ - gazeOffsetY_;
          autoRestCalibrated_ = true;
          data_.restCalibrated = true;
          data_.restCalProgressMs = AUTO_REST_CAL_MS;
          filteredGazeX_ = 0.0f;
          filteredGazeY_ = 0.0f;
        }
      } else {
        autoRestStillSince_ = 0;
        data_.restCalProgressMs = 0;
      }
    } else {
      data_.restCalibrated = true;
      data_.restCalProgressMs = AUTO_REST_CAL_MS;
    }

    const bool strongShake = filteredMotionLevel_ > 0.62f || linearMag > 5.2f || gyroSpeed > 4.3f;
    if (strongShake) {
      if (shakeCandidateAt_ == 0) shakeCandidateAt_ = now;
      if (!shakeLatched_ && (now - shakeCandidateAt_) >= 70) {
        shakeLatched_ = true;
        shakePending_ = true;
      }
    } else {
      shakeCandidateAt_ = 0;
      if (filteredMotionLevel_ < 0.18f) shakeLatched_ = false;
    }
  }

  if (data_.bmpAvailable && (now - lastBmpAt_) >= 1500) {
    lastBmpAt_ = now;
    data_.temperatureC = bmp_.readTemperature();
    data_.pressureHpa = bmp_.readPressure() / 100.0f;
  }
}

bool SensorHub::consumePickup() {
  const bool out = pickupPending_;
  pickupPending_ = false;
  return out;
}

bool SensorHub::consumeShake() {
  const bool out = shakePending_;
  shakePending_ = false;
  return out;
}

bool SensorHub::consumeSettled() {
  const bool out = settledPending_;
  settledPending_ = false;
  return out;
}

bool SensorHub::consumeFall() {
  const bool out = fallPending_;
  fallPending_ = false;
  return out;
}

void SensorHub::setGazeCalibration(float offsetX, float offsetY) {
  gazeOffsetX_ = clampf(offsetX, -1.5f, 1.5f);
  gazeOffsetY_ = clampf(offsetY, -1.5f, 1.5f);
  filteredGazeX_ = 0.0f;
  filteredGazeY_ = 0.0f;
}

bool SensorHub::calibrateCurrent(float& offsetX, float& offsetY) {
  if (!data_.mpuAvailable || !gravityInitialized_) return false;
  offsetX = rawGazeX_;
  offsetY = rawGazeY_;
  sessionRestOffsetX_ = 0.0f;
  sessionRestOffsetY_ = 0.0f;
  setGazeCalibration(offsetX, offsetY);
  autoRestCalibrated_ = true;
  autoRestStillSince_ = 0;
  data_.restCalibrated = true;
  data_.restCalProgressMs = AUTO_REST_CAL_MS;
  return true;
}

void SensorHub::resetCalibration() {
  sessionRestOffsetX_ = 0.0f;
  sessionRestOffsetY_ = 0.0f;
  setGazeCalibration(0.0f, 0.0f);
  // An explicit reset means raw orientation for the rest of this session.
  // Automatic rest calibration will arm again on the next boot.
  autoRestCalibrated_ = true;
  autoRestStillSince_ = 0;
  data_.restCalibrated = true;
  data_.restCalProgressMs = AUTO_REST_CAL_MS;
}
