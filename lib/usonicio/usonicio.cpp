#include "usonicio.hpp"

UltrasonicIO::UltrasonicIO(int _trig, int _echo) : trig(_trig), echo(_echo), initialized_(false) {}

void UltrasonicIO::init() {
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  initialized_ = true;
}

void UltrasonicIO::readUsonic(long* values) {
  if (!initialized_) {
    *values = -1;
    return;
  }
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  // Add timeout to prevent blocking (max 30ms = ~5 meters)
  long duration = pulseIn(echo, HIGH, 30000);
  // Convert to centimeters: (duration * 0.034) / 2
  // Simplified: (duration * 17) / 1000
  duration = (duration * 17) / 1000;
  *values = duration;
  if (duration == 0) {
    duration = -1;  // Set to max on timeout
  }
  return;
}
