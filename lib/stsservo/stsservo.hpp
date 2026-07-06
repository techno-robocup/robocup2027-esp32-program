#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Minimal Feetech SMS/STS bus-servo driver (covers the STS3032).
//
//  The STS3032 speaks a half-duplex TTL UART protocol at 1,000,000 baud by
//  default. This driver builds the raw SMS/STS packets itself, so it pulls in
//  no external libraries.
// ---------------------------------------------------------------------------

// Set to 1 when the servo data line is wired directly to both the MCU TX and
// RX pins (single-wire bus). The MCU then receives an echo of every packet it
// sends, and the driver discards it. Set to 0 when a dedicated half-duplex
// converter board (e.g. Feetech FE-URT-1 / TTL Linker) handles direction.
#ifndef STS_HALF_DUPLEX_ECHO
#define STS_HALF_DUPLEX_ECHO 1
#endif

class STServo {
public:
  // rxPin/txPin are only used on ESP32; ignored on AVR (Mega) where the
  // hardware UART has fixed pins.
  void begin(HardwareSerial& serial, uint32_t baud = 1000000,
             int8_t rxPin = -1, int8_t txPin = -1);

  bool ping(uint8_t id);
  bool enableTorque(uint8_t id, bool on);

  // Position mode: target 0..4095 (~360 deg). speed in steps/s (0 = max),
  // acc 0..255 (0 = max).
  bool writePosition(uint8_t id, uint16_t position,
                     uint16_t speed = 0, uint8_t acc = 0);

  // Encoder / status feedback.
  bool readPosition(uint8_t id, uint16_t& position);
  bool readSpeed(uint8_t id, int16_t& speed);
  bool readVoltage(uint8_t id, uint8_t& deciVolts);   // deciVolts / 10 = volts
  bool readTemperature(uint8_t id, uint8_t& celsius);

  // Writes the ID to EEPROM. Persists across power cycles -- use deliberately.
  bool setID(uint8_t currentID, uint8_t newID);

private:
  HardwareSerial* _serial = nullptr;

  void writeRegisters(uint8_t id, uint8_t addr,
                      const uint8_t* data, uint8_t len);
  bool readRegisters(uint8_t id, uint8_t addr, uint8_t len, uint8_t* out);
  void sendPacket(uint8_t id, uint8_t instr,
                  const uint8_t* params, uint8_t paramLen);
  void flushInput();
  bool readBytes(uint8_t* buf, uint8_t len, uint16_t timeoutMs);
};
