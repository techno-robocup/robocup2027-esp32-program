#include "bnoio.hpp"
#include <Arduino.h>  // Includes FreeRTOS for vTaskDelay

BNOIO::BNOIO()
    : bno(Adafruit_BNO055(55, 0x28))
    , sda_pin_(21)
    , scl_pin_(22)
    , initialized_(false)
    , last_successful_read_(0)
    , consecutive_failures_(0)
    , heading_(0)
    , roll_(0)
    , pitch_(0)
    , accel_x_(0)
    , accel_y_(0)
    , accel_z_(0) {}

BNOIO::BNOIO(int sda_pin, int scl_pin)
    : bno(Adafruit_BNO055(55, 0x28))
    , sda_pin_(sda_pin)
    , scl_pin_(scl_pin)
    , initialized_(false)
    , last_successful_read_(0)
    , consecutive_failures_(0)
    , heading_(0)
    , roll_(0)
    , pitch_(0)
    , accel_x_(0)
    , accel_y_(0)
    , accel_z_(0) {}

bool BNOIO::init() {
  // Initialize I2C bus with specified pins (use standard 400kHz for BNO055)
  Wire.begin(sda_pin_, scl_pin_, 400000);  // 400kHz I2C clock (standard for sensors)
  Wire.setTimeOut(50);                     // 50ms max per I2C transaction to prevent loop() stall

  // Serial.println("[BNO] Attempting bno.begin()...");
  if (!bno.begin()) {
    // Serial.println("[BNO] begin() failed!");
    initialized_ = false;
    return false;
  }
  // Serial.println("[BNO] begin() successful!");

  // Serial.println("[BNO] Setting external crystal...");
  bno.setExtCrystalUse(true);

  // Set to IMU mode (accelerometer + gyroscope fusion, no magnetometer)
  // Serial.println("[BNO] Setting operation mode...");
  bno.setMode(OPERATION_MODE_IMUPLUS);

  // Serial.println("[BNO] Initialization complete!");
  initialized_ = true;
  last_successful_read_ = millis();
  consecutive_failures_ = 0;
  return true;
}

bool BNOIO::readSensor() {
  if (!initialized_) {
    return false;
  }

  sensors_event_t accelerometerData;
  unsigned long now = millis();

  // Check if too much time has passed since last successful read
  if (last_successful_read_ > 0 && (now - last_successful_read_ > 3000)) {
    // Serial.println("[BNO] Timeout detected, attempting recovery...");
    reset();  // Force full reinit
    return false;
  }

  // Read quaternion for reliable yaw in IMUPLUS mode
  imu::Quaternion quat = bno.getQuat();
  bno.getEvent(&accelerometerData, Adafruit_BNO055::VECTOR_ACCELEROMETER);

  // Check if data is valid (BNO055 returns 0 for all values when communication fails)
  // A real sensor should have at least some non-zero acceleration due to gravity
  bool data_valid =
      (accelerometerData.acceleration.x != 0 || accelerometerData.acceleration.y != 0 ||
       accelerometerData.acceleration.z != 0);

  if (!data_valid) {
    consecutive_failures_++;
    // Serial.print("[BNO] Invalid data, failures: ");
    // Serial.println(consecutive_failures_);

    // After 3 consecutive failures, attempt recovery
    if (consecutive_failures_ >= 3) {
      // Serial.println("[BNO] Too many failures, attempting recovery...");
      reset();
      return false;
    }
    return false;
  }

  // Compute Euler angles from quaternion
  double w = quat.w(), x = quat.x(), y = quat.y(), z = quat.z();
  heading_ = atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z)) * 180.0 / M_PI;
  roll_ = asin(2.0 * (w * y - z * x)) * 180.0 / M_PI;
  pitch_ = atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y)) * 180.0 / M_PI;

  accel_x_ = accelerometerData.acceleration.x;
  accel_y_ = accelerometerData.acceleration.y;
  accel_z_ = accelerometerData.acceleration.z;

  last_successful_read_ = now;
  consecutive_failures_ = 0;  // Reset failure counter on success
  return true;
}

float BNOIO::getHeading() const { return heading_; }

float BNOIO::getRoll() const { return roll_; }

float BNOIO::getPitch() const { return pitch_; }

float BNOIO::getAccelX() const { return accel_x_; }

float BNOIO::getAccelY() const { return accel_y_; }

float BNOIO::getAccelZ() const { return accel_z_; }

bool BNOIO::isInitialized() const { return initialized_; }

void BNOIO::reset() {
  // Serial.println("[BNO] Resetting sensor...");
  initialized_ = false;
  consecutive_failures_ = 0;

  // Small delay to let I2C bus settle
  delay(100);

  // Reinitialize I2C bus
  Wire.end();
  delay(50);

  // Reinitialize sensor
  if (init()) {
    // Serial.println("[BNO] Reset successful!");
  } else {
    // Serial.println("[BNO] Reset failed, will retry on next read");
  }
}
