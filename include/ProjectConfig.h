#pragma once
#include <Arduino.h>

namespace hw {
constexpr uint8_t BUTTON_PIN = D1;
constexpr uint8_t BUZZER_PIN = D2;
constexpr uint8_t TOUCH_LEFT_PIN = D3;
constexpr uint8_t TOUCH_RIGHT_PIN = D6;
constexpr bool TOUCH_ACTIVE_HIGH = true; // TTP223-style modules usually idle LOW, touch HIGH.
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr int8_t OLED_RESET_PIN = -1;
constexpr uint16_t SCREEN_WIDTH = 128;
constexpr uint16_t SCREEN_HEIGHT = 64;
constexpr uint32_t I2C_CLOCK_HZ = 400000;
constexpr uint8_t MPU6050_ADDRESS_PRIMARY = 0x68;
constexpr uint8_t MPU6050_ADDRESS_SECONDARY = 0x69;
constexpr uint8_t BMP180_ADDRESS = 0x77;
constexpr bool MPU_SWAP_GAZE_AXES = false;
constexpr float MPU_GAZE_X_SIGN = 1.0f;
constexpr float MPU_GAZE_Y_SIGN = 1.0f;
// Directional shake axes. Flip/swap these if the physical MPU orientation differs.
constexpr bool MPU_SWAP_MOTION_AXES = false;
constexpr float MPU_MOTION_X_SIGN = 1.0f;
constexpr float MPU_MOTION_Y_SIGN = 1.0f;
// Cartoon inertial head tuning. Motion is now a force on one damped spring,
// not a moving target plus jerk, which avoids noisy double-amplification.
constexpr float PET_SPRING_STIFFNESS = 30.0f;
constexpr float PET_SPRING_DAMPING_MOVING = 6.2f;
constexpr float PET_SPRING_DAMPING_REST = 9.4f;
constexpr float PET_MOTION_FORCE_X = 900.0f;
constexpr float PET_MOTION_FORCE_Y = 700.0f;
constexpr float PET_ROTATION_FORCE = 95.0f;
constexpr float PET_MAX_WOBBLE_X = 10.5f;
constexpr float PET_MAX_WOBBLE_Y = 7.0f;
constexpr float PET_MAX_ROTATION = 4.5f;
}

// Most 1.3" 128x64 I2C OLED modules use SH1106.
// Set to 0 if your display is actually SSD1306.
#define OLED_USE_SH1106 1

namespace app {
constexpr char DEVICE_NAME[] = "XIAO Computer Pet";
constexpr char CONFIG_FILE[] = "/pet_config.json";
constexpr char STATE_FILE[] = "/pet_state.json";
constexpr uint16_t CONFIG_VERSION = 13;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint16_t STATUS_SCREEN_MS = 2600;
constexpr uint32_t NEEDS_TICK_MS = 60000;
constexpr uint32_t SENSOR_TASK_MS = 10;
constexpr uint32_t BLE_DIAGNOSTIC_MS = 1000;
}
