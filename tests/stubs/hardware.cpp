#include "Arduino.h"
#include "Modem.h"
#include "WiFiS3.h"
#include <algorithm>
#include <stdexcept>

namespace fake {
uint32_t now = 0;
int pins[32] = {};
int pwm = 0;
int resolution = 8;
bool unsafeDirectionChange = false;
bool unbrakedPwmInitialization = false;
bool pwmInitialized = false;
std::vector<std::string> pinEvents;
bool modemResponds = true;
bool wifiConnected = true;
bool hasIp = true;
bool serverStarted = false;
unsigned modemCalls = 0;
unsigned connectionAttempts = 0;
uint32_t callLatency = 2;
std::string usbDuringNextCall;
std::shared_ptr<Socket> pending;
void modemCall() {
  ++modemCalls;
  now += callLatency;
  for (char c : usbDuringNextCall) Serial.incoming.push_back(c);
  usbDuringNextCall.clear();
}
}

FakeSerial Serial;
FakeWiFi WiFi;
ModemClass modem;
void digitalWrite(int pin, int value) {
  if (pin == 13 && fake::pwm && fake::pins[8] == LOW) fake::unsafeDirectionChange = true;
  fake::pins[pin] = value;
  fake::pinEvents.push_back("digital " + std::to_string(pin) + " " + std::to_string(value));
}
void pinMode(int pin, int mode) {
  // Renesas core 1.6.0 configures the full PFS register. OUTPUT also clears PODR.
  if (mode == OUTPUT) {
    fake::pins[pin] = LOW;
    if (pin == 11) fake::pwm = 0;
  }
  fake::pinEvents.push_back("mode " + std::to_string(pin) + " " + std::to_string(mode));
}
void analogWriteResolution(int bits) { fake::resolution = bits; }
void analogWrite(int pin, int value) {
  if (pin != 11 || value < 0 || value > 4095) throw std::runtime_error("Invalid PWM write");
  if (!fake::pwmInitialized) {
    // The real core starts a new PWM timer at 50% before writing the requested duty.
    fake::unbrakedPwmInitialization = fake::pins[8] != HIGH;
    fake::pwmInitialized = true;
  }
  fake::pwm = value;
  fake::pinEvents.push_back("pwm " + std::to_string(value));
}
void FakeSerial::begin(int baud) { if (baud != 115200) throw std::runtime_error("Wrong baud rate"); }
int FakeSerial::read() {
  if (incoming.empty()) return -1;
  const unsigned char c = static_cast<unsigned char>(incoming.front());
  incoming.pop_front();
  return c;
}
size_t FakeSerial::write(const uint8_t* data, size_t count) {
  if (count > static_cast<size_t>(capacity)) throw std::runtime_error("Blocking serial write");
  outgoing.append(reinterpret_cast<const char*>(data), count);
  return count;
}
void ModemClass::begin(int baud, int retry) {
  if (baud != 115200 || retry != 0) throw std::runtime_error("Blocking modem initialization");
  timeoutMs = 10000;
}
bool ModemClass::write(const std::string&, std::string&, const char*, ...) {
  if (timeoutMs > 50) throw std::runtime_error("Unbounded modem timeout");
  fake::modemCall();
  return fake::modemResponds;
}
int FakeWiFi::begin(const char*, const char*) {
  if (timeoutMs != 0 || !modem.beginned) throw std::runtime_error("Blocking WiFi begin");
  ++fake::connectionAttempts;
  fake::modemCall();
  fake::modemCall();
  return WL_CONNECT_FAILED;  // Actual library returns this when timeout == 0, even if association starts.
}
int FakeWiFi::status() { fake::modemCall(); return fake::wifiConnected ? WL_CONNECTED : WL_CONNECT_FAILED; }
IPAddress FakeWiFi::localIP() { fake::modemCall(); return fake::hasIp ? IPAddress(192, 168, 1, 42) : IPAddress(0, 0, 0, 0); }
WiFiClient WiFiServer::accept() {
  fake::modemCall();
  auto socket = fake::pending;
  fake::pending.reset();
  return WiFiClient(socket);
}
void WiFiServer::begin() { fake::modemCall(); fake::serverStarted = true; }
void WiFiServer::end() { fake::modemCall(); fake::serverStarted = false; }
int WiFiClient::read(uint8_t* buffer, size_t count) {
  fake::modemCall();
  if (!*this) return 0;
  count = std::min(count, std::min(socket_->fragmentSize, socket_->incoming.size()));
  for (size_t i = 0; i < count; ++i) {
    buffer[i] = static_cast<uint8_t>(socket_->incoming.front());
    socket_->incoming.pop_front();
  }
  return static_cast<int>(count);
}
size_t WiFiClient::write(const uint8_t* buffer, size_t count) {
  fake::modemCall();
  if (!*this) return 0;
  socket_->outgoing.append(reinterpret_cast<const char*>(buffer), count);
  return count;
}
void WiFiClient::stop() { fake::modemCall(); if (socket_) socket_->closed = true; }
