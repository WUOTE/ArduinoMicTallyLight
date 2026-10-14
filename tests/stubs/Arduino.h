#pragma once
#include <stdint.h>
#include <stddef.h>
#include <deque>
#include <string>
#include <vector>

constexpr int HIGH = 1;
constexpr int LOW = 0;
constexpr int OUTPUT = 1;
namespace fake {
extern uint32_t now;
extern int pins[32];
extern int pwm;
extern int resolution;
extern bool unsafeDirectionChange;
extern bool unbrakedPwmInitialization;
extern std::vector<std::string> pinEvents;
}
inline uint32_t millis() { return fake::now; }
void digitalWrite(int pin, int value);
void pinMode(int pin, int mode);
void analogWriteResolution(int bits);
void analogWrite(int pin, int value);

struct FakeSerial {
  std::deque<char> incoming;
  std::string outgoing;
  int capacity = 64;
  bool attached = true;
  void begin(int baud);
  explicit operator bool() const { return attached; }
  int available() const { return static_cast<int>(incoming.size()); }
  int read();
  int availableForWrite() const { return capacity; }
  size_t write(const uint8_t* data, size_t count);
};
extern FakeSerial Serial;
