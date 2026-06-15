#include "mutex_guard.hpp"

MutexGuard::MutexGuard(SemaphoreHandle_t sem) : sem(sem) { xSemaphoreTake(sem, portMAX_DELAY); }

MutexGuard::~MutexGuard() { xSemaphoreGive(sem); }
