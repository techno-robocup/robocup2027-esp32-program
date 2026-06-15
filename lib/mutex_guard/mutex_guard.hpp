#pragma once
#include <Arduino.h>

class MutexGuard {
 public:
  MutexGuard(SemaphoreHandle_t sem);
  ~MutexGuard();

 private:
  SemaphoreHandle_t sem;
};