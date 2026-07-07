#pragma once
#ifndef __ROBOT__BUZZERIO__HPP
#define __ROBOT__BUZZERIO__HPP 1
#include <Arduino.h>

// Drives a passive speaker/buzzer with an LEDC PWM square wave.
class BUZZERIO {
 public:
  BUZZERIO();
  BUZZERIO(const std::int8_t& pin, const int& channel);
  BUZZERIO& operator=(const BUZZERIO&) = default;
  bool init_pwm();
  void play(const int& frequency);                          // start a continuous tone
  void beep(const int& frequency, const int& duration_ms);  // blocking tone, then silence
  void stop();                                              // silence the speaker

 private:
  std::int8_t PIN;
  int ledc_channel;
  bool initialized_;
};
#endif
