#pragma once
#include <Arduino.h>

class Message {
 public:
  Message(long long id, const String& message);
  long long getId() const;
  String getMessage() const;

 private:
  long long id;
  String message;
};

class SerialIO {
 public:
  SerialIO();
  void sendMessage(const Message& message);
  Message receiveMessage();
  bool isMessageAvailable();
  void init();

 private:
  bool isReady;
};
