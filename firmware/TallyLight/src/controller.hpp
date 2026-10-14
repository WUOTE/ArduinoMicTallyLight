#pragma once

#include <stdint.h>

namespace tally {

constexpr uint16_t kPwmMax = 4095;
enum class Mode : uint8_t { Off, Idle, Hot, Red, White };
enum class Source : uint8_t { Boot, Usb, Http };
enum class Color : uint8_t { White, Red };

const char* modeName(Mode mode);
const char* sourceName(Source source);
bool parseMode(const char* text, Mode& mode);

// Original two-piece normal: location 1.4, sigma_left .4, sigma_right .75.
float twoPieceNormal(float x);
// Same Gaussian shape, scaled to 0 at each cycle boundary and 1 at the peak.
float breathEnvelope(float x);

struct Settings {
  uint32_t periodMs = 4800;
  uint32_t transitionMs = 250;
  float redFloor = 0;
  float whiteFloor = 0;
};

struct Output {
  Color color;
  uint16_t pwm;
};

class Controller {
 public:
  explicit Controller(Settings settings = Settings());
  void tick(uint32_t now);
  bool setMode(Mode mode, Source source, uint32_t now);
  // Use the last hardware duty, which may lag the animation by a frame.
  bool setMode(Mode mode, Source source, uint32_t now, uint16_t displayedPwm);
  Output output() const;
  Mode mode() const { return mode_; }
  Source source() const { return source_; }
  uint32_t revision() const { return revision_; }
  uint64_t uptimeMs() const { return uptimeMs_; }
  uint32_t phaseMs() const { return phaseMs_; }

 private:
  float targetDuty() const;
  Settings settings_;
  Mode mode_ = Mode::Off;
  Source source_ = Source::Boot;
  uint32_t revision_ = 0;
  uint32_t lastTick_ = 0;
  uint64_t uptimeMs_ = 0;
  uint32_t phaseMs_ = 0;
  uint32_t transitionElapsedMs_ = 0;
  float transitionOffset_ = 0;
};

}  // namespace tally
