#include "buzzerio.hpp"

BUZZERIO::BUZZERIO(const std::int8_t& _pin, const int& _channel)
    : PIN(_pin), ledc_channel(_channel), initialized_(false) {}

BUZZERIO::BUZZERIO() : PIN(-1), ledc_channel(-1), initialized_(false) {}

bool BUZZERIO::init_pwm() {
  if (ledc_channel < 0 || PIN < 0) {
    initialized_ = false;
    return false;
  }
  // Placeholder tone/resolution; play()/beep() re-set the frequency per note.
  if (ledcSetup(ledc_channel, 1000, 8) == 0) {
    initialized_ = false;
    return false;
  }
  ledcAttachPin(PIN, ledc_channel);
  ledcWrite(ledc_channel, 0);  // stay silent until asked to play
  initialized_ = true;
  return true;
}

void BUZZERIO::play(const int& frequency) {
  if (!initialized_) return;
  // ledcWriteTone emits a 50% duty square wave at `frequency` Hz on the channel.
  ledcWriteTone(ledc_channel, frequency);
}

void BUZZERIO::beep(const int& frequency, const int& duration_ms) {
  if (!initialized_) return;
  play(frequency);
  delay(duration_ms);
  stop();
}

void BUZZERIO::stop() {
  if (!initialized_) return;
  ledcWriteTone(ledc_channel, 0);
  ledcWrite(ledc_channel, 0);
}
