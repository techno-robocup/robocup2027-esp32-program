#include <motorio.hpp>

int MOTORIO::channel_counter = 0;

MOTORIO::MOTORIO(const std::int8_t& _PIN, const int& _interval)
    : PIN(_PIN), interval(_interval), ledc_channel(-1), initialized_(false) {
  if (channel_counter >= 8) {
    channel_counter++;
    return;
  }
  ledc_channel = channel_counter;
  channel_counter++;
}

MOTORIO::MOTORIO() : PIN(-1), ledc_channel(-1), interval(0), initialized_(false) {}

bool MOTORIO::init_pwm() {
  if (ledc_channel < 0 || PIN < 0) {
    initialized_ = false;
    return false;
  }
  if (ledcSetup(ledc_channel, 50, 16) == 0) {
    initialized_ = false;
    return false;
  }
  ledcAttachPin(PIN, ledc_channel);
  initialized_ = true;
  return true;
}

void MOTORIO::run_msec(const int& msec) {
  if (!initialized_) return;
  // Convert pulse width in microseconds to PWM duty cycle
  // 50Hz = 20ms (20000us) period
  // Duty cycle = (pulse_width / period) * 65535
  // For 1500us pulse: (1500 / 20000) * 65535 ≈ 4915
  uint32_t duty = (static_cast<uint32_t>(msec) * 65535) / 20000;
  ledcWrite(ledc_channel, duty);
}
