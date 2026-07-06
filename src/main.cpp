#include <Arduino.h>
#include <SMS_STS.h>
#include <VL53L0X.h>
#include <cassert>
#include "armio.hpp"
#include "bnoio.hpp"
#include "buzzerio.hpp"
#include "motorio.hpp"
#include "mutex_guard.hpp"
#include "serialio.hpp"
SerialIO serial;

constexpr int button_pin = 25;

// Passive speaker/buzzer on GPIO2, driven by LEDC channel 5 (0-3 belong to
// MOTORIO, 8-9 to ARMIO, so 4-7 are free).
constexpr std::int8_t buzzer_pin = 2;
constexpr int buzzer_channel = 5;
BUZZERIO buzzer(buzzer_pin, buzzer_channel);
constexpr int8_t PIN_RX = 17;  // ESP32 GPIO16 (RX2) <- FE-URT-2 UART TX
constexpr int8_t PIN_TX = 16;  // ESP32 GPIO17 (TX2) -> FE-URT-2 UART RX

// Feetech STS3032 serial-bus servo IDs driving the four wheels.
const uint8_t motors[] = {1, 2, 3, 4};
constexpr size_t motor_count = sizeof(motors) / sizeof(motors[0]);

SMS_STS sts3032;

BNOIO bnoio;

uint8_t xShutPins[] = {14, 18, 19, 23, 5, 15};
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
constexpr size_t ToF_count = 7;
static_assert(ToF_count == xShut_count + 1, "Mismatch between ToF_count and xShut_count");
VL53L0X ToF[ToF_count];

void setup() {
  serial.init();
  serial.sendMessage(Message(0, "HI"));
  pinMode(button_pin, INPUT);
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
    holdXshut(xShutPins[i]);
  }
  delay(10);  // wait for all ToFs to go down

  for (size_t idx = 0; idx < ToF_count; ++idx) {
    ToF[idx].setTimeout(500);
    bool ok = false;
    for (int attempt = 0; attempt < 10 && !ok; ++attempt) {
      ok = ToF[idx].init();
      if (!ok) {
        delay(50);
      }
    }
    if (ok) {
      ToF[idx].setAddress(0x30 + idx);
    } else {
      serial.sendMessage(Message(0, "ToF " + String(idx) + " init failed"));
    }
    if (idx != xShut_count) {
      releaseXshut(xShutPins[idx]);
    }
  }
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

  if (message.startsWith("MOTOR ")) {
    // Drive every wheel at a fixed test speed for now.
    for (size_t i = 0; i < motor_count; ++i) {
      sts3032.WriteSpe(motors[i], 1000);
    }
    serial.sendMessage(Message(0, "HI"));
  }

  else if (message.startsWith("buzz")) {
    // Sound the speaker: 1kHz tone for 300ms.
    buzzer.beep(1000, 300);
    serial.sendMessage(Message(msg.getId(), "BUZZ"));
  }

  else if (message.startsWith("BNO")) {
    bnoio.readSensor();
    serial.sendMessage(Message(0, String(bnoio.getRoll())));
  }

  else if (message.startsWith("ToF")) {

  }
}
