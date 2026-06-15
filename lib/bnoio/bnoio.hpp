#pragma once
#ifndef __ROBOT__BNOIO__HPP
#define __ROBOT__BNOIO__HPP 1

#include <Adafruit_BNO055.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

class BNOIO {
 public:
  BNOIO();
  BNOIO(int sda_pin, int scl_pin);
  bool init();
  bool readSensor();
  void reset();  // Force reset and reinit

  // Getters for sensor values
  float getHeading() const;
  float getRoll() const;
  float getPitch() const;
  float getAccelX() const;
  float getAccelY() const;
  float getAccelZ() const;

  bool isInitialized() const;

 private:
  Adafruit_BNO055 bno;
  int sda_pin_;
  int scl_pin_;
  bool initialized_;
  unsigned long last_successful_read_;
  unsigned int consecutive_failures_;

  // Cached sensor values
  float heading_;
  float roll_;
  float pitch_;
  float accel_x_;
  float accel_y_;
  float accel_z_;
};

#endif
