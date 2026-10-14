# Implementation and validation

`firmware/TallyLight/src/controller.*` holds the mode state, time tracking and brightness
math without Arduino dependencies. `protocol.*` parses bounded USB lines and incremental
HTTP requests without allocation. `firmware.cpp` owns the bridge, serial buffers and the
WiFiS3 adapter. The Arduino entry point only calls setup and loop.

## Timing and the breathing function

The loop services USB and the light both before and after one network step. Animation
uses elapsed time rather than counting loop iterations, so a delayed frame catches up to
the correct position. Unsigned time differences handle `millis()` wrap; uptime accumulates
in 64 bits. There are no application delays, startup serial waits or complete-request loops.
Changed modes render immediately; ordinary animation frames render at most once per 10 ms.
USB and HTTP pass the last duty actually written to the bridge into a mode change, avoiding
a brightness jump even if a delayed frame has advanced the controller's internal clock.

For `x` in `[0,4)`, the original density is:

```text
sigma = 0.4 if x < 1.4, otherwise 0.75
pdf(x) = sqrt(2/pi) / (0.4 + 0.75) * exp(-0.5 * ((x - 1.4) / sigma)^2)
```

The new animation uses that same Gaussian, with each side normalized from its boundary
value to its peak. This makes the end and start of the cycle meet at the configured floor.
It removes the legacy three-decimal density rounding and 8-bit intermediate conversion.
With the old `20/255` floor, the shape differs by at most one 8-bit count at the 400 original
sample positions; retaining the old rounding instead exactly reproduces all 400 values
(tested). The zero-floor experiment deliberately extends the range below those old samples.
Entering breathing from
a steady/off mode starts the target curve at `x = 1.4`, then follows the slow decay and faster
rise over a 4800 ms cycle. Switching between `hot` and `idle` carries the current phase.

A mode change records the difference between the displayed duty and the new target duty.
That correction decays to zero over `TALLY_TRANSITION_MS` (250 ms by default):

```text
t = min(elapsed / transition_ms, 1)
duty = clamp(target_duty_now + initial_difference * (1 - t*t*(3 - 2*t)), 0, 1)
```

The first output therefore retains the previous brightness. A steady destination reaches
full brightness smoothly; a breathing destination joins its moving curve. Equal-floor
breathing colors keep the same curve, apart from a correction of at most half a PWM count
and any lag since the last hardware frame. Unequal floors settle without a command-time
jump. A new command starts from the currently displayed duty, including an unfinished
transition. Repeated modes do not restart anything. `off` cancels the transition immediately.

Brightness is normalized floating-point duty until `round(duty * 4095)`. Full brightness
is 4095, not 4096. Both per-color floors now default to zero for the bench test. They are duty
ratios: restoring the previous `20/255` floor gives about 321 at 12 bits. Floors apply to the
settled breathing curve; entry from off and changes between different floors can temporarily
go below that floor. The hardware PWM carrier is separate from the animation frame rate and remains
the pinned core's approximately 490 Hz default. See Arduino's [analog implementation](https://github.com/arduino/ArduinoCore-renesas/blob/1.6.0/cores/arduino/analog.cpp)
and [PWM implementation](https://github.com/arduino/ArduinoCore-renesas/blob/1.6.0/cores/arduino/pwm.cpp).

## WiFiS3's remaining synchronous boundary

The application is cooperative, but the stock [WiFiS3 modem transport](https://github.com/arduino/ArduinoCore-renesas/blob/1.6.0/libraries/WiFiS3/src/Modem.cpp)
still waits synchronously for each ESP32 reply. Merely putting `WiFi.begin()` behind a retry
timer would leave its internal multi-second waits intact.

This adapter initializes the modem UART with zero built-in reset retries, then performs up
to three handshakes one second apart, each with a 50 ms response timeout. Failure disables
network work until reboot. It sets `WiFi.setTimeout(0)` to initiate association without
waiting for it, checks connection status once per second, and retries failed connections
after a 15-second backoff. DHCP is polled rather than awaited in a loop. USB control and
animation continue regardless of association, DHCP or HTTP progress.

Most network steps make one modem call; association initiation makes two. These calls can
still cost tens of milliseconds, approximately 100 ms across the two response waits during
a failing connection attempt, plus UART/driver overhead. This is **not a hard real-time
latency guarantee**. Late modem replies and the ESP32 firmware need a physical soak test.
The timeout is configurable from 10 to 250 ms; the default is a starting value to validate
on the actual setup. Check `max_service_gap_ms` while disconnecting Wi-Fi and sending commands.
For control entirely independent of the modem, build with `-UsbOnly`; an empty SSID also
skips all network calls at runtime.

This uses public `Modem` members in Renesas core 1.6.0 to avoid its default 500 ms handshake
retries and 10-second response timeout. Re-check this adapter when upgrading the core;
do not silently switch to an unpinned core. No installed Arduino source is patched.

## Tests and builds

`tools/test-native.cmd` builds and runs the production controller/parser and the complete
firmware against fake Arduino/WiFi devices using MSVC. `tools/test.sh` runs the same tests
under GCC/Clang with address and undefined-behavior sanitizers. The integration scenarios
cover online, offline and absent-modem startup, USB priority, malformed/fragmented HTTP,
backpressure, response lengths, reconnect/DHCP waits and disabled-before-reverse pin ordering.
The core suite checks all 25 mode transitions at multiple brightnesses, interrupted fades,
phase continuity, zero-floor cycles, 400 legacy samples, clock wrap and 10,000
deterministic parser fuzz cases. Fakes test scheduling intent, not physical device behavior.
MSVC's optional AddressSanitizer run is `tools/test-native.cmd asan` (requires that Visual
Studio component); it exercises the same core and integration suites.

`tests/test-command.ps1` runs without Pester or a serial device. It checks port selection,
case normalization, split/stale replies, matching IDs, retries, rejection, timeouts,
connection cleanup, HTTP request/reply handling and the legacy wrapper. Both Windows
PowerShell 5.1 and PowerShell 7 are supported.

`tools/build.ps1` uses a CLI on PATH or the ignored `.tools/arduino-cli/arduino-cli.exe`.
It optionally reads `.tools/arduino-cli.yaml`, writes to `build/wifi` or `build/usb`, and never
uploads. Its `-FactoryDefaults` switch excludes local overrides from the build. CI compiles
both variants against the pinned profile. Upstream core 1.6.0 currently emits warnings with
`--warnings all`; native builds treat warnings in this project's code as errors.

On Windows, the bundled ARM GCC 7 can fail to find `bits/c++config.h` when its installation
path is too long, even though the file exists. Use a short Arduino data/tool path. For this
working copy the official compiler was placed in `.tools/gcc`; the build script recognizes
that path and overrides `build.compiler_path`. The compiler and CLI configuration are local
tools, never committed or required by other checkouts.
