#include <Arduino.h>
#include <cassert>
#include "armio.hpp"
#include "bnoio.hpp"
#include "motorio.hpp"
#include "mutex_guard.hpp"
#include "serialio.hpp"
#include "usonicio.hpp"
SerialIO serial;

/* left right left right */
constexpr int tyre[4] = {13, 14, 15, 16};
constexpr int button_pin = 25;  // Changed from 21 (now used for I2C SDA)
constexpr int arm_feedback = 34, arm_pulse = 17;
constexpr int wire_SIG = 32;
constexpr int ultrasonic_trig1 = 18, ultrasonic_echo1 = 19, ultrasonic_trig2 = 33,
              ultrasonic_echo2 = 23, ultrasonic_trig3 = 26, ultrasonic_echo3 = 27;
constexpr int bno_sda = 21, bno_scl = 22;  // I2C pins for BNO055

constexpr int tyre_interval = 40;

// Fixed array size to match actual usage (4 motors, not 2)
static int tyre_values[4] = {1500, 1500, 1500, 1500};
static int arm_value = 0;
static bool wire = false;

inline const char* readbutton() { return digitalRead(button_pin) ? "ON" : "OFF"; }

MOTORIO tyre_1_motor(tyre[0], tyre_interval), tyre_2_motor(tyre[1], tyre_interval),
    tyre_3_motor(tyre[2], tyre_interval), tyre_4_motor(tyre[3], tyre_interval);

ARMIO arm(arm_pulse, arm_feedback, wire_SIG);

BNOIO bno(bno_sda, bno_scl);

static long ultrasonic_values[3] = {0, 0, 0};
UltrasonicIO ultrasonic_1(ultrasonic_trig1, ultrasonic_echo1),
    ultrasonic_2(ultrasonic_trig2, ultrasonic_echo2),
    ultrasonic_3(ultrasonic_trig3, ultrasonic_echo3);

TaskHandle_t motor_task;
SemaphoreHandle_t motor_sem = xSemaphoreCreateMutex();

void motor_task_func(void* arg) {
  int local_values[4];
  while (true) {
    {
      MutexGuard guard(motor_sem);
      local_values[0] = tyre_values[0];
      local_values[1] = tyre_values[1];
      local_values[2] = tyre_values[2];
      local_values[3] = tyre_values[3];
    }
    tyre_1_motor.run_msec(local_values[0]);
    tyre_2_motor.run_msec(local_values[1]);
    tyre_3_motor.run_msec(local_values[2]);
    tyre_4_motor.run_msec(local_values[3]);
    vTaskDelay(pdMS_TO_TICKS(15));  // ~15ms delay for ~50Hz (20ms period with ~5-6ms pulse time)
  }
}

void stop_running_motor() {
  MutexGuard guard(motor_sem);
  tyre_values[0] = 1500;
  tyre_values[1] = 1500;
  tyre_values[2] = 1500;
  tyre_values[3] = 1500;
}

// Optimized string parsing without String operations
bool parseMotorCommand(const char* message, int* values, int max_values) {
  assert(max_values == 2);
  int idx = 0;
  const char* ptr = message;

  while (idx < max_values && *ptr) {
    // Skip leading spaces
    while (*ptr == ' ') ptr++;
    if (!*ptr) break;

    // Parse number
    int val = 0;
    bool negative = false;

    if (*ptr == '-') {
      negative = true;
      ptr++;
    }

    while (*ptr >= '0' && *ptr <= '9') {
      val = val * 10 + (*ptr - '0');
      ptr++;
    }

    if (negative) val = -val;
    values[idx++] = val;

    // Skip to next space or end
    while (*ptr == ' ') ptr++;
  }

  return idx == max_values;
}

bool parseArmCommand(const char* message, int* armValue, bool* wire) {
  const char* ptr = message;
  int val = 0;
  while (*ptr == ' ') ptr++;
  if (!*ptr) return false;

  while (*ptr >= '0' && *ptr <= '9') {
    val = val * 10 + (*ptr - '0');
    ptr++;
  }
  *armValue = val;
  while (*ptr == ' ') ptr++;
  if (*ptr == '1')
    *wire = true;
  else if (*ptr == '0')
    *wire = false;
  else
    return false;

  return true;
}

void init_with_retry(bool (*init_func)(), int retries = 2, int delay_ms = 100) {
  for (int i = 0; i < retries; i++) {
    if (init_func()) return;
    delay(delay_ms);
  }
}

void setup() {
  serial.init();
  pinMode(button_pin, INPUT);

  // Initialize BNO055 sensor (has its own recovery logic in readSensor)
  if (!bno.init()) {
    delay(100);
    bno.init();
  }

  // Initialize ultrasonic sensors
  ultrasonic_1.init();
  ultrasonic_2.init();
  ultrasonic_3.init();

  // Initialize motor PWM (deferred from constructor to ensure HAL is ready)
  init_with_retry([]() { return tyre_1_motor.init_pwm(); });
  init_with_retry([]() { return tyre_2_motor.init_pwm(); });
  init_with_retry([]() { return tyre_3_motor.init_pwm(); });
  init_with_retry([]() { return tyre_4_motor.init_pwm(); });

  // Initialize arm PWM before setting position
  init_with_retry([]() { return arm.init_pwm(); });
  arm.arm_set_position(2000, false);

  // Create motor task on core 0
  if (xTaskCreatePinnedToCore(motor_task_func, "MotorTask", 2048, nullptr, 1, &motor_task, 0) !=
      pdPASS) {
    delay(100);
    xTaskCreatePinnedToCore(motor_task_func, "MotorTask", 2048, nullptr, 1, &motor_task, 0);
  }
}

int ultrasonic_clock = 0;
char response[64];
unsigned long last_motor_command_time = 0;
constexpr unsigned long motor_timeout_ms = 500;
unsigned long last_bno_read_time = 0;
constexpr unsigned long bno_read_interval_ms = 100;  // Read BNO055 every 100ms max

void loop() {
  // Always read ultrasonic sensors regardless of serial communication
  ++ultrasonic_clock;
  ultrasonic_clock %= 3;
  if (ultrasonic_clock == 0)
    ultrasonic_1.readUsonic(ultrasonic_values + 0);
  else if (ultrasonic_clock == 1)
    ultrasonic_2.readUsonic(ultrasonic_values + 1);
  else if (ultrasonic_clock == 2)
    ultrasonic_3.readUsonic(ultrasonic_values + 2);
  arm.updatePID();

  // Read BNO055 sensor with throttling to prevent I2C bus lockup
  unsigned long now = millis();
  if (now - last_bno_read_time >= bno_read_interval_ms) {
    last_bno_read_time = now;
    bno.readSensor();
  }

  // Check motor timeout
  if (millis() - last_motor_command_time > motor_timeout_ms) {
    stop_running_motor();
  }

  // Check for serial messages
  if (!serial.isMessageAvailable()) return;

  last_motor_command_time = millis();

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
    } else {
      snprintf(response, sizeof(response), "ERR: MOTOR");
      serial.sendMessage(Message(msg.getId(), String(response)));
    }
  } else if (message.startsWith("Rescue ")) {
    const char* rescue = message.c_str() + 7;

    if (parseArmCommand(rescue, &arm_value, &wire)) {
      arm.arm_set_position(arm_value, wire);
      snprintf(response, sizeof(response), "OK %d %d", arm_value, (int)wire);
      serial.sendMessage(Message(msg.getId(), String(response)));
    } else {
      snprintf(response, sizeof(response), "ERR: Rescue");
      serial.sendMessage(Message(msg.getId(), String(response)));
    }
  } else if (message.startsWith("GET button")) {
    const char* status = readbutton();
    if (strcmp(status, "OFF") == 0) stop_running_motor();
    serial.sendMessage(Message(msg.getId(), status));
  } else if (message.startsWith("GET usonic")) {
    snprintf(response, sizeof(response), "%ld %ld %ld", ultrasonic_values[0], ultrasonic_values[1],
             ultrasonic_values[2]);
    serial.sendMessage(Message(msg.getId(), String(response)));
  } else if (message.startsWith("GET bno")) {
    if (bno.isInitialized()) {
      snprintf(response, sizeof(response), "%.2f %.2f %.2f %.2f %.2f %.2f", bno.getHeading(),
               bno.getRoll(), bno.getPitch(), bno.getAccelX(), bno.getAccelY(), bno.getAccelZ());
      serial.sendMessage(Message(msg.getId(), String(response)));
    } else {
      snprintf(response, sizeof(response), "ERR: BNO055 not initialized");
      serial.sendMessage(Message(msg.getId(), String(response)));
    }
  } else if (message.startsWith("healthcheck")) {
    serial.sendMessage(Message(msg.getId(), "OK"));
  } else {
    return;
  }
}
