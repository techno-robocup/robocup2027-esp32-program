# GitHub Copilot Instructions for robocup2026-esp32-kanto

## Project Overview

This repository contains ESP32 firmware for the RoboCup 2026 Kanto project. It's an embedded robotics system that controls motors, servos, and sensors on an ESP32 microcontroller using the Arduino framework.

## Technology Stack

- **Platform**: ESP32 (espressif32)
- **Framework**: Arduino
- **Build System**: PlatformIO
- **Language**: C++ (C++11 standard with Arduino extensions)
- **Key Libraries**: 
  - ESP32Servo (v3.0.8) for servo control
  - FreeRTOS (included with ESP32 Arduino core) for task management

## Project Structure

```
├── src/                    # Main application code
│   └── main.cpp           # Entry point with setup() and loop()
├── lib/                    # Custom libraries (project-specific)
│   ├── armio/             # Arm servo control with PD controller
│   ├── motorio/           # Motor PWM control
│   ├── mutex_guard/       # RAII mutex wrapper for thread safety
│   ├── serialio/          # Serial communication with message protocol
│   └── usonicio/          # Ultrasonic sensor interface
├── include/               # Public header files (currently minimal)
├── test/                  # Test directory
├── platformio.ini         # PlatformIO configuration
├── .clang-format          # Code formatting rules
└── format.sh              # Script to format all code files
```

## Build and Development

### Building and Uploading

```bash
# Build the project
pio run

# Upload to ESP32
pio run --target upload

# Open serial monitor
pio device monitor
```

### Code Formatting

Always format code before committing using the provided script:

```bash
./format.sh
```

This runs `clang-format` on all `.cpp`, `.hpp`, `.cc`, `.cxx`, and `.h` files using the Google style guide configuration.

## Code Style and Conventions

### General Guidelines

- **Style Guide**: Google C++ Style Guide with customizations defined in `.clang-format`
- **Indentation**: 2 spaces (no tabs)
- **Column Limit**: 100 characters
- **Pointer Alignment**: Left-aligned (e.g., `int* ptr` not `int *ptr`)
- **Braces**: Attached style (opening brace on same line)
- **Includes**: Sorted alphabetically

### Header Guards

Use both `#pragma once` and traditional include guards:

```cpp
#pragma once
#ifndef __ROBOT__MODULENAME__HPP
#define __ROBOT__MODULENAME__HPP 1
// ... code ...
#endif
```

### Class Naming and Structure

- **Class Names**: Use UPPERCASE for main I/O classes (e.g., `MOTORIO`, `ARMIO`, `SerialIO`)
- **Member Variables**: Private members with descriptive names (e.g., `arm_pulse_pin`, `prev_msec`)
- **Constructor**: Provide both default constructor and parameterized constructor
- **Copy Assignment**: Use `= default` for trivial copy assignment operators

Example:
```cpp
class MOTORIO {
 public:
  MOTORIO();
  MOTORIO(const std::int8_t&, const int&);
  MOTORIO& operator=(const MOTORIO&) = default;
  virtual void run_msec(const int&);
  
 private:
  std::int8_t PIN;
  unsigned long prev_msec;
  int interval;
};
```

### Constants and Variables

- Use `constexpr` for compile-time constants (e.g., pin numbers, timing intervals)
- Use `const` for runtime constants and pass-by-reference parameters
- Prefer `std::int8_t` for pin numbers
- Use `unsigned long` for time values from `millis()` to handle overflow

Example:
```cpp
constexpr int button_pin = 21;
constexpr int arm_feedback = 34, arm_pulse = 17;
```

### FreeRTOS and Concurrency

- Use FreeRTOS tasks for concurrent operations (e.g., motor control loop)
- Protect shared data with mutexes (`SemaphoreHandle_t`)
- Use RAII pattern with `MutexGuard` class for automatic mutex release
- Use `vTaskDelay(pdMS_TO_TICKS(ms))` for task delays

Example:
```cpp
void motor_task_func(void* arg) {
  int local_values[4];
  while (true) {
    {
      MutexGuard guard(motor_sem);  // Locks mutex, auto-releases on scope exit
      local_values[0] = tyre_values[0];
      // ... copy more values
    }
    // ... use local_values
    vTaskDelay(pdMS_TO_TICKS(15));
  }
}
```

### Arduino-Specific Patterns

- Use `pinMode()`, `digitalWrite()`, `digitalRead()` for GPIO
- Use `analogRead()` for ADC (0-4095 on ESP32)
- Use `millis()` for non-blocking timing
- Serial communication: Initialize with `Serial.begin(9600)` (baud rate: 9600)

### Memory and Performance

- Minimize String operations in performance-critical code
- Use C-style string parsing for efficiency (see `parseMotorCommand` in main.cpp)
- Prefer stack allocation over heap allocation where possible
- Use `assert()` for debug-time invariant checking

### Comments

- Use comments sparingly; prefer self-documenting code
- Add comments for non-obvious algorithms (e.g., PID controllers, timing calculations)
- Document hardware-specific constants and pin mappings

## Hardware Configuration

Current hardware setup (defined in main.cpp):

- **Motors**: 4 tyres on pins 13, 14, 15, 16 (PWM control, 40ms interval)
- **Button**: Pin 21 (input)
- **Arm Servo**: Pulse on pin 17, feedback on pin 34
- **Wire Sensor**: Pin 32
- **Ultrasonic Sensors**: 3 sensors on pins (18,19), (22,23), (26,27) for trig/echo pairs
- **Serial Monitor**: 9600 baud

## Testing

- Test directory exists at `test/`
- Always test on actual hardware when modifying timing-critical code
- Verify motor control, sensor readings, and serial communication after changes

## Common Pitfalls to Avoid

1. **Time Overflow**: Always use `unsigned long` for `millis()` values, not `int`
2. **Race Conditions**: Always protect shared data accessed from multiple tasks with mutexes
3. **Blocking Delays**: Never use `delay()` in tasks; use `vTaskDelay()` instead
4. **String Memory**: Avoid excessive String concatenation; use C-style strings in loops
5. **Array Bounds**: Ensure array sizes match actual usage (e.g., if controlling 4 motors, declare array as `int values[4]`)

## Additional Notes

- The project is designed for real-time robot control with strict timing requirements
- PWM signals for servos typically require ~50Hz (20ms period) with pulse widths 1000-2000μs
- Motor control runs in a separate FreeRTOS task for predictable timing
- Serial protocol uses Message class with ID and payload for structured communication
