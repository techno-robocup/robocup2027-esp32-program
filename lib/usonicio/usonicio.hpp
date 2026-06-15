#pragma once
#include <Arduino.h>

class UltrasonicIO {
 public:
  UltrasonicIO(int _trig, int _echo);
  void init();
  void readUsonic(long*);

 private:
  int trig, echo;
  bool initialized_;
};
