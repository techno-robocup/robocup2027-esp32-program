#pragma once
#ifndef __ROBOT__ARMIO__HPP
#define __ROBOT__ARMIO__HPP 1
#include <Arduino.h>

class ARMIO {
 public:
  ARMIO();
  ARMIO(const std::int8_t& arm_pulse, const std::int8_t& arm_feedback, const std::int8_t& wire_sig);
  ARMIO& operator=(const ARMIO&) = default;
  bool init_pwm();
  void arm_set_position(const int& position, const bool& enable);
  void updatePID();

 private:
  std::int8_t arm_pulse_pin;
  std::int8_t arm_feedback_pin;
  std::int8_t wire_sig_pin;

  int arm_pulse_channel;
  int wire_sig_channel;

  // PID controller variables
  const float kp = 0.3;
  const float ki = 0.01;
  const float kd = 0.18;
  float previous_error;
  float integral_sum;
  int target_position;
  bool initialized_;

  // Read current arm position from feedback pin
  int getCurrentPosition();
};

#endif
