#include <Arduino.h>
#include <cassert>
#include "armio.hpp"
#include "bnoio.hpp"
#include "motorio.hpp"
#include "mutex_guard.hpp"
#include "serialio.hpp"
SerialIO serial;

void setup() {
  serial.init();
  pinMode(button_pin, INPUT);
}

void loop() {
  Message msg = serial.receiveMessage();
  String message = msg.getMessage();

  if (message.startsWith("MOTOR ")) {
    const char* motor_data = message.c_str() + 6;  // Skip "MOTOR "
    int temp_values[2];
    if (parseMotorCommand(motor_data, temp_values, 2)) {
      // TODO: Fix legacy code for assuming 2 motor values
      {
        MutexGuard guard(motor_sem);
        tyre_values[0] = temp_values[0];
        tyre_values[1] = 1500 - (temp_values[1] - 1500);
        tyre_values[2] = tyre_values[0];
        tyre_values[3] = tyre_values[1];
      }
      snprintf(response, sizeof(response), "OK %d %d %d %d", tyre_values[0], tyre_values[1],
               tyre_values[2], tyre_values[3]);
      serial.sendMessage(Message(msg.getId(), String(response)));
    }
}
