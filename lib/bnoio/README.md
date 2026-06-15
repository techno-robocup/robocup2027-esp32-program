# BNO055 I/O Library

## Overview

This library provides an interface for communicating with the BNO055 9-axis absolute orientation sensor via I2C. The sensor is configured to operate in **IMU mode**, which uses accelerometer and gyroscope data with sensor fusion (no magnetometer).

## Hardware Configuration

- **I2C SDA Pin**: 21
- **I2C SCL Pin**: 22
- **I2C Address**: 0x28 (default)
- **Operating Mode**: IMUPLUS (accelerometer + gyroscope fusion)

## Features

The library reads and caches the following sensor data:
- **Orientation (Euler angles)**:
  - Heading (yaw) - rotation around Z-axis (0-360°)
  - Roll - rotation around Y-axis (-180 to +180°)
  - Pitch - rotation around X-axis (-90 to +90°)
- **Acceleration**:
  - X-axis acceleration (m/s²)
  - Y-axis acceleration (m/s²)
  - Z-axis acceleration (m/s²)

### Automatic Error Recovery

The library includes robust error detection and recovery:
- **Data Validation**: Checks if sensor data is valid (detects when all values are zero)
- **Automatic Reinitialization**: After 3 consecutive failures, the sensor and I2C bus are automatically reset
- **Timeout Protection**: If no successful read occurs for 3 seconds, forces a complete reset
- **No Manual Reset Required**: The system recovers automatically without needing to power cycle the ESP32

## Protocol Command

### GET bno

Retrieves the current sensor values.

**Request**: `<id> GET bno`

**Response**: `<id> <heading> <roll> <pitch> <accel_x> <accel_y> <accel_z>`

Example:
- Request: `1 GET bno`
- Response: `1 45.23 10.50 -5.30 0.10 0.05 9.81`

**Error Response**: `<id> ERR: BNO055 not initialized`

## Usage

```cpp
#include "bnoio.hpp"

// Create BNO055 object with I2C pins
BNOIO bno(21, 22);

// Initialize in setup()
void setup() {
  if (!bno.init()) {
    Serial.println("BNO055 initialization failed!");
  }
}

// Read sensor in loop()
void loop() {
  bno.readSensor();
  
  float heading = bno.getHeading();
  float roll = bno.getRoll();
  float pitch = bno.getPitch();
  float accel_x = bno.getAccelX();
  float accel_y = bno.getAccelY();
  float accel_z = bno.getAccelZ();
}
```
