#include <Arduino.h>
#include <HX711.h>
#include <SMS_STS.h>
#include <VL53L0X.h>
#include <cassert>
#include "bnoio.hpp"
#include "buzzerio.hpp"
#include "mutex_guard.hpp"
#include "serialio.hpp"
SerialIO serial;

constexpr int8_t SWITCH_PIN = 25;

// TODO:
//  Passive speaker/buzzer on GPIO2, driven by LEDC channel 5 (0-3 belong to
//  MOTORIO, 8-9 to ARMIO, so 4-7 are free).
constexpr std::int8_t BUZZER_PIN = 2;
constexpr int BUZZER_CHANEL = 5;
constexpr int8_t PIN_RX = 17;  // ESP32 GPIO16 (RX2) <- FE-URT-2 UART TX
constexpr int8_t PIN_TX = 16;  // ESP32 GPIO17 (TX2) -> FE-URT-2 UART RX

// Feetech STS3032 serial-bus servo IDs driving the four wheels.
const uint8_t motors[] = {1, 2, 3, 4};
const uint8_t motor_L[2] = {1, 2};
const uint8_t motor_R[2] = {3, 4};
constexpr size_t motor_count = sizeof(motors) / sizeof(motors[0]);

constexpr int8_t LOAD_L_PIN[2] = {26, 33};
constexpr int8_t LOAD_R_PIN[2] = {34, 27};

constexpr int8_t CAGE_PIN = 32;

constexpr int8_t PHOTO_PIN = 4;

// uint8_t xShutPins[] = {-1 ,14, 18, 19, 23, 5, 15}; -1 is default Address (do not use XSHUT)
uint8_t xShutPins[] = {14, 23, 15, 19};
uint8_t tof_L[] = {0};
uint8_t tof_R[] = {1};
uint8_t tof_F[] = {2, 3};
constexpr size_t xShut_count = sizeof(xShutPins) / sizeof(xShutPins[0]);

// Release XSHUT by going high-impedance and letting the board pull-up raise
// it; XSHUT must never be driven HIGH directly.
static void releaseXshut(uint8_t pin) {
  pinMode(pin, INPUT);
  delay(10);
}

static void holdXshut(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

// ToF[0] is the always-on sensor (no XSHUT wire); ToF[i] for i >= 1 is the
// sensor on xShutPins[i - 1].
constexpr size_t ToF_count = 4;
// static_assert(ToF_count == xShut_count + 1, "Mismatch between ToF_count and xShut_count");
static_assert(ToF_count == xShut_count, "Mismatch between ToF_count and xShut_count");

SMS_STS sts3032;
VL53L0X ToF[ToF_count];
BNOIO bnoio;
HX711 load_R, load_L;
BUZZERIO buzzer(BUZZER_PIN, BUZZER_CHANEL);

void setup() {
  serial.init();
  serial.sendMessage(Message(0, "HI"));
  pinMode(SWITCH_PIN, INPUT);
  buzzer.init_pwm();

  Serial2.begin(1000000, SERIAL_8N1, PIN_RX, PIN_TX);  // servo bus @ 1 Mbaud
  sts3032.pSerial = &Serial2;
  delay(50);  // to wait for initialization of sts3032

  // Put every servo on the bus into continuous-rotation (wheel) mode.
  // WheelMode() writes SMS_STS_MODE to EEPROM, so unlock/lock around it.
  for (size_t i = 0; i < motor_count; ++i) {
    uint8_t id = motors[i];
    sts3032.unLockEprom(id);
    sts3032.EnableTorque(id, 1);
    sts3032.WheelMode(id);
    sts3032.LockEprom(id);
  }

  bnoio.init();

  // I2C is already up: bnoio.init() called Wire.begin(21, 22, 400000).
  // Hold every XSHUT-wired sensor in reset so only ToF[0] answers at the
  // default address 0x29, then bring them up one at a time and re-address.
  for (size_t i = 0; i < xShut_count; ++i) {
    if (xShutPins[i] >= 0) {
      holdXshut(xShutPins[i]);
    }
  }
  delay(10);
  uint8_t nextAddress = 0x30;

  for (size_t idx = 0; idx < ToF_count; ++idx) {
    if (idx < xShut_count && xShutPins[idx] >= 0) {
      releaseXshut(xShutPins[idx]);
    }

    ToF[idx].setTimeout(500);
    bool ok = false;

    for (int attempt = 0; attempt < 10 && !ok; ++attempt) {
      ok = ToF[idx].init();
      if (!ok) {
        delay(50);
      }
    }

    if (ok) {
      if (idx < xShut_count && xShutPins[idx] < 0) {
        serial.sendMessage(Message(0, "ToF " + String(idx) + " init ok (Default Address: 0x29)"));
      } else {
        ToF[idx].setAddress(nextAddress);
        serial.sendMessage(Message(0, "ToF " + String(idx) + " init ok (New Address: 0x" +
                                          String(nextAddress, HEX) + ")"));

        nextAddress++;
      }
    } else {
      serial.sendMessage(Message(0, "ToF " + String(idx) + " init failed"));
    }
  }

  load_R.begin(LOAD_R_PIN[0], LOAD_R_PIN[1]);
  load_L.begin(LOAD_L_PIN[0], LOAD_L_PIN[1]);
  load_R.reset();
  load_L.reset();

  pinMode(CAGE_PIN, OUTPUT);
  pinMode(PHOTO_PIN, INPUT);
}

void loop() {
  // Heartbeat: prints once a second no matter what, so you can confirm the USB
  // serial link works without sending anything. If you never see TICK, the
  // problem is the monitor/port/baud, not the command parsing.
  static unsigned long last_beat = 0;
  static uint32_t beats = 0;
  if (millis() - last_beat >= 1000) {
    last_beat = millis();
    serial.sendMessage(Message(0, "TICK " + String(beats++)));
  }

  Message msg = serial.receiveMessage();
  String message = msg.getMessage();

  // Echo whatever arrived so you can see exactly how it was parsed: the leading
  // integer becomes the id, everything after the first space is the message.
  if (message.length() > 0) {
    serial.sendMessage(Message(msg.getId(), "RX [" + message + "]"));
  }

  if (message.startsWith("MOTOR")) {
    int16_t motor_L = 0;
    int16_t motor_R = 0;

    if (sscanf(message.c_str(), "MOTOR %hd %hd", &motor_L, &motor_R) == 2) {
      if (motor_L < -9999 || motor_L > 9999 || motor_R < -9999 || motor_R > 9999) {
        serial.sendMessage(Message(msg.getId(), "MOTOR out of range"));
      } else {
        uint8_t left_servos[2] = {1, 2};
        for (int i = 0; i < 2; ++i) {
          sts3032.WriteSpe(left_servos[i], motor_L);
        }
        uint8_t right_servos[2] = {3, 4};
        for (int i = 0; i < 2; ++i) {
          sts3032.WriteSpe(right_servos[i], motor_R);
        }

        serial.sendMessage(Message(msg.getId(), "ok"));
      }
    } else {
      serial.sendMessage(Message(msg.getId(), "Invalid format"));
    }
  }

  else if (message.startsWith("ARM")) {
    int arm_pos = 0;
    if (sscanf(message.c_str(), "ARM %d", &arm_pos) == 1) {
      if (arm_pos < 0 || arm_pos > 4095) {
        serial.sendMessage(Message(msg.getId(), "ARM out of range"));
      } else {
        sts3032.WritePosEx(5, arm_pos, 1000);
        serial.sendMessage(Message(msg.getId(), "ok"));
      }
    } else {
      serial.sendMessage(Message(msg.getId(), "Invalid format"));
    }
  }

  else if (message.startsWith("BUZZ")) {
    uint16_t freq = 0;
    uint16_t dur = 0;
    if (sscanf(message.c_str(), "BUZZ %hu %hu", &freq, &dur) == 2) {
      buzzer.beep(freq, dur);
      serial.sendMessage(Message(msg.getId(), "ok"));
    } else {
      serial.sendMessage(Message(msg.getId(), "Invalid format"));
    }
  }

  else if (message.startsWith("BNO")) {
    if (bnoio.isInitialized()) {
      serial.sendMessage(Message(
          msg.getId(), String(bnoio.getHeading()) + " " + String(bnoio.getRoll()) + " " +
                           String(bnoio.getPitch()) + " " + String(bnoio.getAccelX()) + " " +
                           String(bnoio.getAccelY()) + " " + String(bnoio.getAccelZ())));
    } else {
      serial.sendMessage(Message(msg.getId(), "BNO not initialized"));
    }
  }

  else if (message.startsWith("TOF")) {
    char dir;
    if (sscanf(message.c_str(), "TOF %c", &dir) == 1) {
      if (dir == 'l') {
        for (size_t i = 0; i < sizeof(tof_L); ++i) {
          uint16_t distances[sizeof(tof_L)];
          if (tof_L[i] < ToF_count) {
            distances[i] = ToF[tof_L[i]].readRangeSingleMillimeters();
          } else {
            distances[i] = 0;
          }
        }
      } else if (dir == 'r') {
        for (size_t i = 0; i < sizeof(tof_R); ++i) {
          uint16_t distances[sizeof(tof_R)];
          if (tof_R[i] < ToF_count) {
            distances[i] = ToF[tof_R[i]].readRangeSingleMillimeters();
          } else {
            distances[i] = 0;
          }
        }
      } else if (dir == 'f') {
        for (size_t i = 0; i < sizeof(tof_F); ++i) {
          uint16_t distances[sizeof(tof_F)];
          if (tof_F[i] < ToF_count) {
            distances[i] = ToF[tof_F[i]].readRangeSingleMillimeters();
          } else {
            distances[i] = 0;
          }
        }
      } else {
        serial.sendMessage(Message(msg.getId(), "Invalid direction"));
      }
    } else {
      serial.sendMessage(Message(msg.getId(), "Invalid format"));
    }
  }

  else if (message.startsWith("LOAD")) {
    long long load_L_val = load_L.get_units(10) / 1000;
    long long load_R_val = load_R.get_units(10) / 1000;
    serial.sendMessage(Message(msg.getId(), String(load_L_val) + " " + String(load_R_val)));
  }

  else if (message.startsWith("CAGE")) {
    char mes;
    if (sscanf(message.c_str(), "CAGE %c", &mes) == 1) {
      if (mes == 'O') {
        digitalWrite(CAGE_PIN, HIGH);
        serial.sendMessage(Message(msg.getId(), "ok"));
      } else if (mes == 'C') {
        digitalWrite(CAGE_PIN, LOW);
        serial.sendMessage(Message(msg.getId(), "ok"));
      } else {
        serial.sendMessage(Message(msg.getId(), "Invalid format"));
      }
    } else {
      serial.sendMessage(Message(msg.getId(), "Invalid format"));
    }
  }

  else if (message.startsWith("SWITCH")) {
    bool switch_state = digitalRead(SWITCH_PIN);
    serial.sendMessage(
        Message(msg.getId(), (String("ok") + " " + String(switch_state ? "on" : "off"))));
  }

  else if (message.startsWith("PHOTO")) {
    uint16_t photo_state = digitalRead(PHOTO_PIN);
    serial.sendMessage(Message(msg.getId(), (String("ok") + " " + String(photo_state))));
  }

  else {
    serial.sendMessage(Message(msg.getId(), "Unknown: [" + message + "]"));
  }
}
