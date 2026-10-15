#include "controller.hpp"

#include <math.h>

namespace tally {
namespace {
float clamp01(float value) { return value < 0 ? 0 : (value > 1 ? 1 : value); }
uint32_t boundedDuration(uint32_t value, uint32_t minimum) {
  return value < minimum ? minimum : (value > 600000U ? 600000U : value);
}
bool breathing(Mode mode) { return mode == Mode::Idle || mode == Mode::Hot; }
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
  // Validate each segment independently. Holds never consume time from the fades.
  // Bounding segments also keeps the derived period and phase arithmetic in range.
  settings_.riseMs = boundedDuration(settings_.riseMs, 1);
  settings_.peakHoldMs = boundedDuration(settings_.peakHoldMs, 0);
  settings_.fallMs = boundedDuration(settings_.fallMs, 1);
  settings_.floorHoldMs = boundedDuration(settings_.floorHoldMs, 0);
  if (settings_.transitionMs < 10 || settings_.transitionMs > 5000) settings_.transitionMs = 250;
  if (!isfinite(settings_.redFloor)) settings_.redFloor = 0;
  if (!isfinite(settings_.whiteFloor)) settings_.whiteFloor = 0;
  settings_.redFloor = clamp01(settings_.redFloor);
  settings_.whiteFloor = clamp01(settings_.whiteFloor);
}

void Controller::tick(uint32_t now) {
  const uint32_t elapsed = now - lastTick_;  // Unsigned subtraction survives millis() wrap.
  lastTick_ = now;
  uptimeMs_ += elapsed;
  if (breathing(mode_)) {
    const uint32_t period = periodMs();
    phaseMs_ = (phaseMs_ + elapsed % period) % period;
  }
  const uint32_t remaining = settings_.transitionMs - transitionElapsedMs_;
  transitionElapsedMs_ += elapsed < remaining ? elapsed : remaining;
}

bool Controller::setMode(Mode mode, Source source, uint32_t now) {
  tick(now);
  return setMode(mode, source, now, output().pwm);
}

bool Controller::setMode(Mode mode, Source source, uint32_t now, uint16_t displayedPwm) {
  tick(now);
  source_ = source;
  if (mode == mode_) return false;  // Retries never restart a breath or transition.
  const bool wasBreathing = breathing(mode_);
  mode_ = mode;
  ++revision_;
  if (breathing(mode_) && !wasBreathing) {
    phaseMs_ = settings_.riseMs;  // Enter at the start of the peak hold.
  }
  // Carry the phase between breathing colors. A short correction starts the new
  // curve/solid at the actual displayed duty, including interrupted transitions
  // and different per-color floors. Off always cancels the transition immediately.
  transitionOffset_ = mode_ == Mode::Off ? 0 :
      clamp01(static_cast<float>(displayedPwm) / kPwmMax) - targetDuty();
  transitionElapsedMs_ = mode_ == Mode::Off ? settings_.transitionMs : 0;
  return true;
}

float Controller::targetDuty() const {
  if (mode_ == Mode::Off) return 0;
  if (!breathing(mode_)) return 1;
  const float floor = mode_ == Mode::Hot ? settings_.redFloor : settings_.whiteFloor;
  const uint32_t rise = settings_.riseMs;
  const uint32_t peakEnd = rise + settings_.peakHoldMs;
  const uint32_t fallEnd = peakEnd + settings_.fallMs;
  // Holds are regions of the cycle clock, never blocking waits. A late frame can
  // cross any number of regions without stretching the cycle or delaying commands.
  if (phaseMs_ >= fallEnd) return floor;
  if (phaseMs_ >= rise && phaseMs_ < peakEnd) return 1;
  const float x = phaseMs_ < rise ?
      1.4f * static_cast<float>(phaseMs_) / rise :
      1.4f + 2.6f * static_cast<float>(phaseMs_ - peakEnd) / settings_.fallMs;
  return floor + (1.0f - floor) * breathEnvelope(x);
}

Output Controller::output() const {
  const bool red = mode_ == Mode::Hot || mode_ == Mode::Red;
  Output result{red ? Color::Red : Color::White, 0};
  if (mode_ == Mode::Off) return result;
  const float t = static_cast<float>(transitionElapsedMs_) / settings_.transitionMs;
  const float eased = t * t * (3.0f - 2.0f * t);
  const float duty = targetDuty() + transitionOffset_ * (1.0f - eased);
  // Quantize only the final duty sent to the bridge.
  result.pwm = static_cast<uint16_t>(lroundf(clamp01(duty) * kPwmMax));
  return result;
}

}  // namespace tally
