#include "firmware.hpp"
#include "Arduino.h"
#include "Modem.h"
#include "WiFiS3.h"
#include <iostream>
#include <stdexcept>

#define CHECK(expr) do { if (!(expr)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " " #expr); } while (false)

void runFor(uint32_t duration) {
  const uint32_t start = fake::now;
  while (fake::now - start < duration) { ++fake::now; tally::loopFirmware(); }
}
void usb(const std::string& text) {
  for (char c : text) Serial.incoming.push_back(c);
  tally::loopFirmware();
}
std::shared_ptr<fake::Socket> http(const std::string& text, size_t fragment = 64) {
  auto socket = std::make_shared<fake::Socket>();
  socket->fragmentSize = fragment;
  for (char c : text) socket->incoming.push_back(c);
  fake::pending = socket;
  return socket;
}
std::string post(const std::string& body, const std::string& token = "test-token") {
  return "POST /tally HTTP/1.1\r\nContent-Type: application/json\r\nAuthorization: Bearer " + token +
    "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}
void waitClosed(const std::shared_ptr<fake::Socket>& socket) {
  const uint32_t start = fake::now;
  while (!socket->closed && fake::now - start < 4000) runFor(1);
  CHECK(socket->closed);
}

int main(int argc, char** argv) {
  try {
    const std::string scenario = argc > 1 ? argv[1] : "online";
    fake::modemResponds = scenario != "missing-modem";
    fake::wifiConnected = scenario != "offline";
    tally::setupFirmware();
    CHECK(fake::pwm == 0 && fake::pins[8] == HIGH && fake::resolution == 12);
    CHECK(fake::pinEvents.front() == "digital 8 1");
    usb("@first hot\n");
    CHECK(fake::pwm == 4095 && fake::pins[13] == LOW && fake::pins[8] == LOW);
    runFor(100);
    CHECK(Serial.outgoing.find("\"id\":\"first\"") != std::string::npos);
    CHECK(fake::pwm < 4095);
    usb("hot\n");
    CHECK(fake::pwm < 4095);  // Same-mode retry does not restart.
    usb("idle\n");
    CHECK(fake::pwm == 4095 && fake::pins[13] == HIGH && fake::pins[8] == LOW);
    usb("off\n");
    CHECK(fake::pwm == 0 && fake::pins[8] == HIGH);
    usb("white\n");
    CHECK(fake::pwm == 4095 && fake::pins[13] == HIGH);
    // USB transmit backpressure must not hold up light changes.
    Serial.capacity = 0;
    for (int i = 0; i < 20; ++i) usb("status\n");
    usb("red\n");
    CHECK(fake::pwm == 4095 && fake::pins[13] == LOW);
    Serial.capacity = 64;
    runFor(4000);
    if (scenario == "missing-modem" || scenario == "offline") {
      CHECK(!fake::serverStarted);
      const unsigned calls = fake::modemCalls;
      usb("idle\n");
      CHECK(fake::pwm == 4095 && fake::pins[13] == HIGH);
      runFor(5000);
      if (scenario == "missing-modem") CHECK(fake::modemCalls == calls);
      usb("off\n");
      CHECK(fake::pwm == 0);
    } else {
      CHECK(fake::serverStarted);
      auto socket = http(post("{\"mode\":\"hot\"}"), 7);
      waitClosed(socket);
      CHECK(socket->outgoing.find("HTTP/1.1 200 OK") == 0);
      CHECK(fake::pins[13] == LOW && fake::pwm > 3000);
      const auto bodyStart = socket->outgoing.find("\r\n\r\n") + 4;
      const auto lengthStart = socket->outgoing.find("Content-Length: ") + 16;
      CHECK(std::stoul(socket->outgoing.substr(lengthStart)) == socket->outgoing.size() - bodyStart);
      // The command takes effect before all response chunks have been sent.
      socket = http(post("{\"mode\":\"white\"}"));
      while (fake::pins[13] != HIGH) runFor(1);
      CHECK(!socket->closed && fake::pwm == 4095);
      waitClosed(socket);
      runFor(5000);
      CHECK(fake::pwm == 4095);  // Old USB hot/red state cannot override HTTP white.
      socket = http(post("{\"mode\":\"off\"}", "wrong"));
      waitClosed(socket);
      CHECK(socket->outgoing.find("HTTP/1.1 401") == 0 && fake::pwm == 4095);
      socket = http(post("{\"mode\":\"off\",\"mode\":\"hot\"}"));
      waitClosed(socket);
      CHECK(socket->outgoing.find("HTTP/1.1 400") == 0 && fake::pwm == 4095);
      // A stalled HTTP sender cannot starve an unrelated USB command.
      socket = http("POST /tally HTTP/1.1\r\nContent-Length: 14\r\n");
      runFor(100);
      usb("off\n");
      CHECK(fake::pwm == 0);
      waitClosed(socket);
      CHECK(socket->outgoing.find("HTTP/1.1 408") == 0);
      // A command arriving during an RPC is applied immediately after that call returns.
      fake::callLatency = 50;
      fake::usbDuringNextCall = "red\n";
      runFor(6);
      CHECK(fake::pwm == 4095 && fake::pins[13] == LOW);
      fake::callLatency = 2;
      fake::wifiConnected = false;
      runFor(1200);
      CHECK(!fake::serverStarted);
      usb("idle\n");
      CHECK(fake::pwm == 4095 && fake::pins[13] == HIGH);
      fake::wifiConnected = true;
      fake::hasIp = false;
      runFor(17000);
      usb("off\n");
      CHECK(fake::pwm == 0);  // DHCP pending still permits USB.
      fake::hasIp = true;
      runFor(1500);
      CHECK(fake::serverStarted && fake::connectionAttempts >= 2);
      socket = http("GET /status HTTP/1.1\r\nAuthorization: Bearer test-token\r\n\r\n");
      waitClosed(socket);
      CHECK(socket->outgoing.find("\"wifi\":\"online\"") != std::string::npos);
    }
    CHECK(!fake::unsafeDirectionChange);
    std::cout << "Firmware integration scenario passed: " << scenario << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
