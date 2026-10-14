#include "controller.hpp"

#include <math.h>

namespace tally {
namespace {
float clamp01(float value) { return value < 0 ? 0 : (value > 1 ? 1 : value); }
bool equalIgnoringCase(const char* a, const char* b) {
  while (*a && *b) {
    const char c = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a + ('a' - 'A')) : *a;
    if (c != *b) return false;
    ++a;
    ++b;
  }
  return *a == *b;
}
float gaussian(float x) {
  const float sigma = x < 1.4f ? 0.4f : 0.75f;
  const float z = (x - 1.4f) / sigma;
  return expf(-0.5f * z * z);
}
}  // namespace

const char* modeName(Mode mode) {
  switch (mode) {
    case Mode::Off: return "off";
    case Mode::Idle: return "idle";
    case Mode::Hot: return "hot";
    case Mode::Red: return "red";
    case Mode::White: return "white";
  }
  return "off";
}

const char* sourceName(Source source) {
  switch (source) {
    case Source::Boot: return "boot";
    case Source::Usb: return "usb";
    case Source::Http: return "http";
  }
  return "boot";
}

bool parseMode(const char* text, Mode& mode) {
  constexpr Mode modes[] = {Mode::Off, Mode::Idle, Mode::Hot, Mode::Red, Mode::White};
  for (const Mode candidate : modes) {
    if (equalIgnoringCase(text, modeName(candidate))) {
      mode = candidate;
      return true;
    }
  }
  return false;
}

float twoPieceNormal(float x) { return 0.6938126616f * gaussian(x); }

float breathEnvelope(float x) {
  // Endpoint normalization removes the tiny discontinuity where the cycle wraps.
  // The old table additionally rounded density to 3 decimal places, then to 8 bits.
  constexpr float leftEdge = 0.0021874911f;   // exp(-.5 * (1.4/.4)^2)
  constexpr float rightEdge = 0.0024568165f;  // exp(-.5 * (2.6/.75)^2)
  const float edge = x < 1.4f ? leftEdge : rightEdge;
  return clamp01((gaussian(x) - edge) / (1.0f - edge));
}

Controller::Controller(Settings settings) : settings_(settings) {
  if (settings_.periodMs < 100 || settings_.periodMs > 600000) settings_.periodMs = 4800;
  if (!isfinite(settings_.redFloor)) settings_.redFloor = 20.0f / 255.0f;
  if (!isfinite(settings_.whiteFloor)) settings_.whiteFloor = 20.0f / 255.0f;
  settings_.redFloor = clamp01(settings_.redFloor);
  settings_.whiteFloor = clamp01(settings_.whiteFloor);
}

void Controller::tick(uint32_t now) {
  const uint32_t elapsed = now - lastTick_;  // Unsigned subtraction survives millis() wrap.
  lastTick_ = now;
  uptimeMs_ += elapsed;
  if (mode_ == Mode::Idle || mode_ == Mode::Hot) {
    phaseMs_ = (phaseMs_ + elapsed % settings_.periodMs) % settings_.periodMs;
  }
}

bool Controller::setMode(Mode mode, Source source, uint32_t now) {
  tick(now);
  source_ = source;
  if (mode == mode_) return false;  // Retries never restart a breath.
  mode_ = mode;
  ++revision_;
  phaseMs_ = settings_.periodMs * 35U / 100U;  // Peak is at 1.4 / 4 of a cycle.
  return true;
}

Output Controller::output() const {
  const bool red = mode_ == Mode::Hot || mode_ == Mode::Red;
  Output result{red ? Color::Red : Color::White, 0};
  if (mode_ == Mode::Off) return result;
  if (mode_ == Mode::Red || mode_ == Mode::White) {
    result.pwm = kPwmMax;
    return result;
  }
  const float floor = red ? settings_.redFloor : settings_.whiteFloor;
  const float x = 4.0f * static_cast<float>(phaseMs_) / static_cast<float>(settings_.periodMs);
  const float duty = floor + (1.0f - floor) * breathEnvelope(x);
  // The only brightness quantization in the new animation.
  result.pwm = static_cast<uint16_t>(lroundf(clamp01(duty) * kPwmMax));
  return result;
}

}  // namespace tally
