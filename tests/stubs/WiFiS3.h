#pragma once
#include "Arduino.h"
#include <memory>

constexpr int WL_CONNECTED = 3;
constexpr int WL_CONNECT_FAILED = 4;
class IPAddress {
 public:
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes_{a, b, c, d} {}
  uint8_t operator[](size_t i) const { return bytes_[i]; }
  bool operator==(const IPAddress& other) const {
    for (size_t i = 0; i < 4; ++i) if (bytes_[i] != other.bytes_[i]) return false;
    return true;
  }
 private:
  uint8_t bytes_[4];
};
namespace fake {
struct Socket {
  std::deque<char> incoming;
  std::string outgoing;
  bool closed = false;
  size_t fragmentSize = 64;
};
extern bool modemResponds;
extern bool wifiConnected;
extern bool hasIp;
extern bool serverStarted;
extern unsigned modemCalls;
extern unsigned connectionAttempts;
extern uint32_t callLatency;
extern std::string usbDuringNextCall;
extern std::shared_ptr<Socket> pending;
void modemCall();
}
class WiFiClient {
 public:
  WiFiClient() = default;
  explicit WiFiClient(std::shared_ptr<fake::Socket> socket) : socket_(socket) {}
  explicit operator bool() const { return socket_ && !socket_->closed; }
  int read(uint8_t* buffer, size_t count);
  size_t write(const uint8_t* buffer, size_t count);
  void stop();
 private:
  std::shared_ptr<fake::Socket> socket_;
};
class WiFiServer {
 public:
  explicit WiFiServer(int port) { (void)port; }
  WiFiClient accept();
  void begin();
  void end();
  explicit operator bool() const { return fake::serverStarted; }
};
struct FakeWiFi {
  uint32_t timeoutMs = 10000;
  void setTimeout(uint32_t value) { timeoutMs = value; }
  int begin(const char*, const char*);
  int status();
  IPAddress localIP();
};
extern FakeWiFi WiFi;
