#include "serialio.hpp"

SerialIO::SerialIO() : isReady(false) { init(); }

// 115200 baud: ~4 ms MOTOR round-trip (vs ~90 ms at 4800), enabling smooth
// ~50 Hz control from the Raspberry Pi. The host esp32_driver must match.
void SerialIO::init() { Serial.begin(115200); }

void SerialIO::sendMessage(const Message& message) {
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "%lld %s", message.getId(), message.getMessage().c_str());
  Serial.println(buffer);
  return;
}

Message SerialIO::receiveMessage() {
  long long id = -1;
  String message = "";

  while (Serial.available()) {
    String str = Serial.readStringUntil('\n');
    if (str.length() > 0) {
      int id_end = str.indexOf(' ');
      if (id_end > 0) {
        // Optimized: Use c_str() for parsing to avoid additional String
        // allocations
        const char* str_ptr = str.c_str();
        char* end_ptr;
        id = strtoll(str_ptr, &end_ptr, 10);
        if (id_end < str.length() - 1) {
          message = str.substring(id_end + 1);
        }
      }
    }
  }
  return Message(id, message);
}

bool SerialIO::isMessageAvailable() { return Serial.available() > 0; }

Message::Message(long long Id, const String& Message) : id(Id), message(Message) {}

long long Message::getId() const { return id; }

String Message::getMessage() const { return message; }
