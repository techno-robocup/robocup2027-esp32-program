#include "stsservo.hpp"

namespace {
// SMS/STS instructions.
constexpr uint8_t INST_PING  = 0x01;
constexpr uint8_t INST_READ  = 0x02;
constexpr uint8_t INST_WRITE = 0x03;

// SMS/STS memory table (subset). The STS series is little-endian.
constexpr uint8_t REG_ID           = 0x05;  // 5   EEPROM
constexpr uint8_t REG_LOCK         = 0x37;  // 55  1 = EEPROM write-protected
constexpr uint8_t REG_TORQUE       = 0x28;  // 40
constexpr uint8_t REG_ACC          = 0x29;  // 41  start of the position block
constexpr uint8_t REG_PRESENT_POS  = 0x38;  // 56
constexpr uint8_t REG_PRESENT_SPD  = 0x3A;  // 58
constexpr uint8_t REG_VOLTAGE      = 0x3E;  // 62
constexpr uint8_t REG_TEMPERATURE  = 0x3F;  // 63
}  // namespace

void STServo::begin(HardwareSerial& serial, uint32_t baud,
                    int8_t rxPin, int8_t txPin) {
  _serial = &serial;
#if defined(ARDUINO_ARCH_ESP32)
  if (rxPin >= 0 && txPin >= 0) {
    _serial->begin(baud, SERIAL_8N1, rxPin, txPin);
  } else {
    _serial->begin(baud);
  }
#else
  (void)rxPin;
  (void)txPin;
  _serial->begin(baud);
#endif
}

void STServo::flushInput() {
  while (_serial->available()) _serial->read();
}

bool STServo::readBytes(uint8_t* buf, uint8_t len, uint16_t timeoutMs) {
  uint32_t start = millis();
  uint8_t got = 0;
  while (got < len) {
    if (_serial->available()) {
      buf[got++] = static_cast<uint8_t>(_serial->read());
    } else if ((millis() - start) > timeoutMs) {
      return false;
    }
  }
  return true;
}

void STServo::sendPacket(uint8_t id, uint8_t instr,
                         const uint8_t* params, uint8_t paramLen) {
  uint8_t length = paramLen + 2;            // params + instruction + checksum
  uint8_t checksum = id + length + instr;

  flushInput();

  _serial->write(static_cast<uint8_t>(0xFF));
  _serial->write(static_cast<uint8_t>(0xFF));
  _serial->write(id);
  _serial->write(length);
  _serial->write(instr);
  for (uint8_t i = 0; i < paramLen; i++) {
    _serial->write(params[i]);
    checksum += params[i];
  }
  _serial->write(static_cast<uint8_t>(~checksum));
  _serial->flush();                         // block until the bytes are out

#if STS_HALF_DUPLEX_ECHO
  // On a single-wire bus the MCU receives a copy of what it just sent.
  uint8_t echo[20];
  readBytes(echo, paramLen + 6, 20);        // total frame length = paramLen + 6
#endif
}

void STServo::writeRegisters(uint8_t id, uint8_t addr,
                             const uint8_t* data, uint8_t len) {
  uint8_t params[12];
  params[0] = addr;
  for (uint8_t i = 0; i < len; i++) params[i + 1] = data[i];
  sendPacket(id, INST_WRITE, params, len + 1);

  // Consume the servo's status (ACK) packet so it cannot desync later reads.
  uint8_t ack[6];
  readBytes(ack, 6, 50);
}

bool STServo::readRegisters(uint8_t id, uint8_t addr,
                            uint8_t len, uint8_t* out) {
  uint8_t params[2] = {addr, len};
  sendPacket(id, INST_READ, params, 2);

  // Status packet: 0xFF 0xFF ID LENGTH ERROR [DATA...] CHECKSUM
  uint8_t header[5];
  if (!readBytes(header, 5, 50)) return false;
  if (header[0] != 0xFF || header[1] != 0xFF) return false;
  if (header[2] != id) return false;
  if (header[3] != len + 2) return false;            // LENGTH = data + 2

  uint8_t tail[20];                                  // data bytes + checksum
  if (!readBytes(tail, len + 1, 50)) return false;

  uint8_t checksum = header[2] + header[3] + header[4];
  for (uint8_t i = 0; i < len; i++) {
    checksum += tail[i];
    out[i] = tail[i];
  }
  return static_cast<uint8_t>(~checksum) == tail[len];
}

bool STServo::ping(uint8_t id) {
  sendPacket(id, INST_PING, nullptr, 0);
  uint8_t reply[6];
  if (!readBytes(reply, 6, 50)) return false;
  return reply[0] == 0xFF && reply[1] == 0xFF && reply[2] == id;
}

bool STServo::enableTorque(uint8_t id, bool on) {
  uint8_t v = on ? 1 : 0;
  writeRegisters(id, REG_TORQUE, &v, 1);
  return true;
}

bool STServo::writePosition(uint8_t id, uint16_t position,
                            uint16_t speed, uint8_t acc) {
  if (position > 4095) position = 4095;
  // Contiguous block reg 41..47: ACC, goal pos (L/H), goal time (L/H = 0),
  // goal speed (L/H). This mirrors Feetech's "write position" command.
  uint8_t data[7] = {
      acc,
      static_cast<uint8_t>(position & 0xFF),
      static_cast<uint8_t>((position >> 8) & 0xFF),
      0,
      0,
      static_cast<uint8_t>(speed & 0xFF),
      static_cast<uint8_t>((speed >> 8) & 0xFF),
  };
  writeRegisters(id, REG_ACC, data, 7);
  return true;
}

bool STServo::readPosition(uint8_t id, uint16_t& position) {
  uint8_t buf[2];
  if (!readRegisters(id, REG_PRESENT_POS, 2, buf)) return false;
  position = static_cast<uint16_t>(buf[0]) |
             (static_cast<uint16_t>(buf[1]) << 8);
  return true;
}

bool STServo::readSpeed(uint8_t id, int16_t& speed) {
  uint8_t buf[2];
  if (!readRegisters(id, REG_PRESENT_SPD, 2, buf)) return false;
  uint16_t raw = static_cast<uint16_t>(buf[0]) |
                 (static_cast<uint16_t>(buf[1]) << 8);
  // Sign-magnitude: bit 15 is the direction sign.
  speed = (raw & 0x8000) ? -static_cast<int16_t>(raw & 0x7FFF)
                         : static_cast<int16_t>(raw);
  return true;
}

bool STServo::readVoltage(uint8_t id, uint8_t& deciVolts) {
  return readRegisters(id, REG_VOLTAGE, 1, &deciVolts);
}

bool STServo::readTemperature(uint8_t id, uint8_t& celsius) {
  return readRegisters(id, REG_TEMPERATURE, 1, &celsius);
}

bool STServo::setID(uint8_t currentID, uint8_t newID) {
  uint8_t unlock = 0;
  writeRegisters(currentID, REG_LOCK, &unlock, 1);   // unlock EEPROM
  delay(10);
  writeRegisters(currentID, REG_ID, &newID, 1);
  delay(10);
  uint8_t lock = 1;
  writeRegisters(newID, REG_LOCK, &lock, 1);         // re-lock EEPROM
  delay(10);
  return ping(newID);
}
