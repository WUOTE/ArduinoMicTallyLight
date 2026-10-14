#include "controller.hpp"
#include "protocol.hpp"
#include "fixtures/legacy_breath.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace tally;
static unsigned checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " " #expr); } while (false)

void transitions() {
  const Mode modes[] = {Mode::Off, Mode::Idle, Mode::Hot, Mode::Red, Mode::White};
  for (Mode from : modes) for (Mode to : modes) {
    Controller light;
    light.setMode(from, Source::Usb, 0);
    light.tick(2400);
    const auto phase = light.phaseMs();
    const auto before = light.output();
    const auto revision = light.revision();
    CHECK(light.setMode(to, Source::Http, 2400) == (from != to));
    CHECK(light.mode() == to);
    CHECK(light.source() == Source::Http);
    CHECK(light.revision() == revision + (from != to ? 1 : 0));
    if (from == to) {
      CHECK(light.phaseMs() == phase);
      CHECK(light.output().pwm == before.pwm);
    } else {
      CHECK(light.output().pwm == (to == Mode::Off ? 0 : 4095));
    }
    CHECK(light.output().color == ((to == Mode::Hot || to == Mode::Red) ? Color::Red : Color::White));
  }
  Controller light;
  CHECK(light.output().pwm == 0);
  light.setMode(Mode::Hot, Source::Usb, 1);
  light.setMode(Mode::White, Source::Http, 2);
  light.tick(10000);
  CHECK(light.output().pwm == 4095);  // Previous USB breathing cannot resume over HTTP solid white.
  light.setMode(Mode::Off, Source::Usb, 10001);
  light.tick(100000);
  CHECK(light.output().pwm == 0);
}

void timeAndCurve() {
  Controller regular, delayed;
  regular.setMode(Mode::Hot, Source::Usb, 0);
  delayed.setMode(Mode::Hot, Source::Usb, 0);
  for (uint32_t t = 1; t <= 123456; ++t) regular.tick(t);
  delayed.tick(123456);
  CHECK(regular.output().pwm == delayed.output().pwm);
  CHECK(regular.phaseMs() == delayed.phaseMs());
  Controller wrap;
  wrap.setMode(Mode::Hot, Source::Usb, UINT32_MAX - 20U);
  const auto phase = wrap.phaseMs();
  wrap.tick(29);
  CHECK(wrap.phaseMs() == (phase + 50) % 4800);
  CHECK(wrap.uptimeMs() == static_cast<uint64_t>(UINT32_MAX) + 30);
  CHECK(std::fabs(breathEnvelope(0)) < 0.000001f);
  CHECK(std::fabs(breathEnvelope(4)) < 0.000001f);
  CHECK(breathEnvelope(1.4f) == 1);
  float previous = 0;
  for (int i = 0; i <= 1400; ++i) {
    const float current = breathEnvelope(static_cast<float>(i) / 1000);
    CHECK(current + 0.000001f >= previous);
    previous = current;
  }
  for (int i = 1400; i <= 4000; ++i) {
    const float current = breathEnvelope(static_cast<float>(i) / 1000);
    CHECK(current <= previous + 0.000001f);
    previous = current;
  }
  bool seen[4096] = {};
  Controller light;
  light.setMode(Mode::Idle, Source::Usb, 0);
  for (uint32_t t = 0; t < 4800; ++t) {
    light.tick(t);
    const auto pwm = light.output().pwm;
    CHECK(pwm >= 321 && pwm <= 4095);
    seen[pwm] = true;
  }
  unsigned unique = 0;
  for (bool value : seen) if (value) ++unique;
  CHECK(unique > 1000);  // No hidden 8-bit quantization before the final PWM write.
  light.tick(4800);
  CHECK(light.output().pwm == 4095);
  Settings calibration;
  calibration.redFloor = 0.2f;
  calibration.whiteFloor = 0.1f;
  Controller calibrated(calibration);
  calibrated.setMode(Mode::Hot, Source::Usb, 0);
  calibrated.tick(3120);
  CHECK(calibrated.output().pwm == 819);
  calibrated.setMode(Mode::Idle, Source::Usb, 3120);
  calibrated.tick(6240);
  CHECK(calibrated.output().pwm == 410);
  calibrated.setMode(Mode::Off, Source::Usb, 6240);
  CHECK(calibrated.output().pwm == 0);  // Floors never apply to off.
}

void legacyDistribution() {
  for (int i = 0; i < 400; ++i) {
    const float x = static_cast<float>(i) / 100;
    const int density = static_cast<int>(std::lround(1000 * twoPieceNormal(x)));
    const int legacy = 20 + ((density - 2) * 235 + 346) / 692;
    CHECK(legacy == kLegacyBreath[i]);  // Exactly the original table with its original rounding.
    const int smooth8 = static_cast<int>(std::lround(20 + 235 * breathEnvelope(x)));
    CHECK(std::abs(smooth8 - kLegacyBreath[i]) <= 1);
  }
}

void serialProtocol() {
  Mode mode = Mode::Red;
  CHECK(parseMode("HoT", mode) && mode == Mode::Hot);
  CHECK(!parseMode("unknown", mode) && mode == Mode::Hot);
  auto command = parseCommand("  @abc_123 HOT\r ");
  CHECK(command.kind == CommandKind::SetMode && command.mode == Mode::Hot);
  CHECK(std::string(command.id) == "abc_123");
  CHECK(parseCommand("status").kind == CommandKind::Status);
  CHECK(parseCommand("PING").kind == CommandKind::Ping);
  for (const auto* invalid : {"", "@ hot", "@12345678901234567 hot", "@a\" hot", "red white", "hot garbage", "@abc wat"})
    CHECK(parseCommand(invalid).kind == CommandKind::Invalid);
  CHECK(std::string(parseCommand("@abc wat").id) == "abc");
  SerialLine line;
  for (char c : std::string("hot\r")) CHECK(line.feed(c, 0) == LineEvent::None);
  CHECK(line.feed('\n', 1) == LineEvent::Ready);
  CHECK(parseCommand(line.text()).mode == Mode::Hot);
  for (int i = 0; i < 70; ++i) line.feed('x', 2);
  CHECK(line.feed('\n', 3) == LineEvent::Invalid);
  for (char c : std::string("off")) line.feed(c, 4);
  CHECK(line.feed('\n', 5) == LineEvent::Ready);
  CHECK(parseCommand(line.text()).mode == Mode::Off);
  line.feed('r', UINT32_MAX - 10U);
  CHECK(!line.expire(500));
  CHECK(line.expire(1000));
  line.feed('e', 1001);
  line.feed('d', 1001);
  CHECK(line.feed('\n', 1002) == LineEvent::Invalid);
  CHECK(!line.expire(10000));
  line.feed('r', 10000);
  line.feed('\0', 10000);
  CHECK(line.feed('\n', 10000) == LineEvent::Invalid);
}

HttpRequest readRequest(const std::string& text, const char* token = "") {
  HttpRequest request;
  request.reset(0, token);
  for (char c : text) request.feed(c);
  return request;
}

std::string post(const std::string& body) {
  return "POST /tally HTTP/1.1\r\nHost: tally\r\nContent-Type: application/json\r\nContent-Length: " +
    std::to_string(body.size()) + "\r\n\r\n" + body;
}

void httpProtocol() {
  const std::string valid = post("{\"mode\":\"hot\"}");
  for (size_t split = 0; split < valid.size(); ++split) {
    HttpRequest request;
    request.reset(100);
    for (size_t i = 0; i < split; ++i) request.feed(valid[i]);
    CHECK(!request.done());
    request.expire(200);
    for (size_t i = split; i < valid.size(); ++i) request.feed(valid[i]);
    CHECK(request.done() && request.error() == 0);
    CHECK(request.method() == HttpMethod::Post && request.path() == HttpPath::Tally);
    CHECK(request.jsonContentType() && request.authorized());
    Mode mode;
    CHECK(parseModeJson(request.body(), mode) && mode == Mode::Hot);
  }
  CHECK(readRequest("GET /status HTTP/1.1\r\n\r\n").done());
  CHECK(readRequest("GET /statusOops HTTP/1.1\r\n\r\n").path() == HttpPath::Other);
  CHECK(!readRequest("GET /status HTTP/1.1\r\n\r\n", "secret").authorized());
  CHECK(readRequest("GET /status HTTP/1.1\r\nAuthorization: Bearer secret\r\n\r\n", "secret").authorized());
  CHECK(!readRequest("GET /status HTTP/1.1\r\nAuthorization: Bearer secre\r\n\r\n", "secret").authorized());
  const std::string prefix = "POST /tally HTTP/1.1\r\n";
  CHECK(readRequest(prefix + "\r\n").error() == 411);
  CHECK(readRequest(prefix + "Content-Length: 97\r\n\r\n").error() == 413);
  CHECK(readRequest(prefix + "Content-Length: 999999999999999999999\r\n\r\n").error() == 413);
  CHECK(readRequest(prefix + "Content-Length: -1\r\n\r\n").error() == 400);
  CHECK(readRequest(prefix + "Content-Length: 0\r\nContent-Length: 0\r\n\r\n").error() == 400);
  CHECK(readRequest(prefix + "Transfer-Encoding: chunked\r\n\r\n").error() == 400);
  CHECK(readRequest(prefix + "Expect: 100-continue\r\n\r\n").error() == 417);
  CHECK(readRequest(prefix + "Content-Length : 0\r\n\r\n").error() == 400);
  CHECK(readRequest(prefix + "broken\r\n\r\n").error() == 400);
  CHECK(readRequest(prefix + "X: " + std::string(200, 'a') + "\r\n\r\n").error() == 431);
  std::string tooMany = prefix;
  for (int i = 0; i < 200; ++i) tooMany += "X: a\r\n";
  CHECK(readRequest(tooMany).error() == 431);
  CHECK(readRequest("GET / HTTP/1.1\n\n").error() == 400);
  CHECK(readRequest("GET / HTTP/1.1\rX").error() == 400);
  CHECK(readRequest(prefix + "Authorization: a\r\nAuthorization: b\r\n\r\n").error() == 400);
  CHECK(readRequest(prefix + "Content-Type: a\r\nContent-Type: b\r\n\r\n").error() == 400);
  CHECK(readRequest(prefix + "content-type: APPLICATION/JSON; charset=utf-8\r\ncontent-length: 0\r\n\r\n").jsonContentType());
  auto request = readRequest(prefix + "Content-Length: 14\r\n\r\n{\"mode\":");
  CHECK(!request.done());
  request.expire(1499);
  CHECK(!request.done());
  request.feed(' ');
  request.expire(1500);
  CHECK(request.error() == 408);
  request.reset(UINT32_MAX - 1000U);
  request.expire(499);
  CHECK(request.error() == 408);
  Mode mode = Mode::White;
  CHECK(parseModeJson(" \n { \"mode\" : \"hot\" } \t", mode) && mode == Mode::Hot);
  for (const auto* invalid : {"", "{", "{}", "{\"mode\":123}", "{\"mode\":\"bogus\"}",
      "{\"mode\":\"off\",\"mode\":\"hot\"}", "{\"mode\":\"hot\",\"extra\":true}",
      "{\"mode\":\"hot\"}garbage", "{\"mode\":\"h\\u006ft\"}", "{\"modex\":\"hot\"}",
      "{\"mode\":\"hot\",}", "[\"hot\"]", "null"}) {
    mode = Mode::White;
    CHECK(!parseModeJson(invalid, mode));
    CHECK(mode == Mode::White);  // Invalid requests never leak a partially parsed mode.
  }
}

void fuzzParsers() {
  uint32_t random = 0x12345678;
  for (unsigned trial = 0; trial < 10000; ++trial) {
    HttpRequest request;
    request.reset(trial);
    SerialLine line;
    std::string bytes;
    for (unsigned i = 0; i < trial % 600; ++i) {
      random = random * 1664525U + 1013904223U;
      const char c = static_cast<char>(random >> 24);
      request.feed(c);
      line.feed(c, trial);
      if (c) bytes.push_back(c);
    }
    Mode mode = Mode::Off;
    parseModeJson(bytes.c_str(), mode);
    parseCommand(bytes.c_str());
    request.expire(trial + 1500);
    CHECK(request.done());
  }
}

int main() {
  try {
    transitions();
    timeAndCurve();
    legacyDistribution();
    serialProtocol();
    httpProtocol();
    fuzzParsers();
    std::cout << "Core/protocol tests passed (" << checks << " checks, 10,000 parser fuzz cases).\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
