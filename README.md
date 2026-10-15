# Arduino microphone tally light

Local USB and optional Wi-Fi control of a Yellowtec m!ka red/white on-air light using an
**Arduino UNO R4 WiFi + Arduino Motor Shield Rev3, channel B**.

The firmware boots with the light off. Every valid command immediately updates the shared
state. Mode changes keep the current PWM brightness, switch color, and smoothly join the
new mode over 250 ms. Switching between breathing colors also preserves the cycle's position.
Repeating the current mode preserves both the breath and any transition already in progress.
The last valid command processed wins, whether it came from USB or HTTP.

| Command | Light |
| --- | --- |
| `off` | Off immediately |
| `idle` | White breathing, continuing from the current brightness |
| `hot` | Red breathing, continuing from the current brightness |
| `red` | Fade from the current brightness to steady full red |
| `white` | Fade from the current brightness to steady full white |

Breathing uses the original two-piece Gaussian shape. The default settings give a
**6-second cycle**: 1.68 s rise, 0.4 s at full brightness, 3.12 s fall, and 0.8 s off.
Animation updates at 100 Hz
with 12-bit PWM (`0..4095`). No lookup table is used in production. Both breathing
floors are **zero**: each cycle can go completely dark. The owner reports no flicker at this
floor on the current setup. If the lamp flickers or cuts out abruptly near zero, raise
`TALLY_RED_FLOOR` or `TALLY_WHITE_FLOOR` in
`config.h`; the previous setting was `(20.0f / 255.0f)`, about 7.84% duty.
`TALLY_TRANSITION_MS` controls the 250 ms transition. `off` still takes effect immediately.

The cycle length is calculated automatically from its four segments. Changing a hold
extends or shortens the cycle without changing either fade's speed. Holds do not block
commands. Set both holds to zero to remove the plateaus.

## Tuning the settings

Edit the breathing block at the top of
[`firmware/TallyLight/config.h`](firmware/TallyLight/config.h). All times are milliseconds
(1000 = 1 second). These are the current settings:

```cpp
#define TALLY_RISE_MS       1680U
#define TALLY_PEAK_HOLD_MS  400U
#define TALLY_FALL_MS       3120U
#define TALLY_FLOOR_HOLD_MS 800U
```

For a different peak or off hold, change only that number. There is no total period to
maintain and no need to adjust the rise or fall to make room for holds. Zero disables a hold.

`config.h` also contains the zero brightness floors and the 250 ms mode transition setting.
`src/controller.hpp` contains no tuning defaults. Keep Wi-Fi credentials in `config.local.h`;
`config.example.h` is only a credentials template. Save `config.h`, then click **Upload** in
Arduino IDE to compile and flash the changes, or use the command-line steps below.

## Upload

1. Open [`firmware/TallyLight/TallyLight.ino`](firmware/TallyLight/TallyLight.ino) in Arduino IDE.
2. Install **Arduino UNO R4 Boards 1.6.0**, select **Arduino UNO R4 WiFi**, and select its port.
3. Click **Upload**, which compiles and flashes the sketch. **Verify** only compiles.
   Sources inside `src/` are compiled automatically; the small `.ino` is the entry point.
   A clean checkout works over USB with no Wi-Fi configuration.

For Wi-Fi, copy [`config.example.h`](firmware/TallyLight/config.example.h) to
`firmware/TallyLight/config.local.h` and fill in the SSID and password. These local headers
are ignored by Git. An existing migrated checkout already has its credentials preserved
in an ignored `arduino_secrets.h`, included by `config.local.h`; keep that local file.
The ESP32 connectivity firmware must support the WiFiS3 library shipped with the pinned core.
If Wi-Fi is unavailable, USB and the light continue working.

For a reproducible command-line build, install [Arduino CLI](https://arduino.github.io/arduino-cli/latest/installation/)
(validated with **1.5.1**) and run:

```powershell
./tools/build.ps1
./tools/build.ps1 -UsbOnly
```

Or on any supported CLI platform:

```sh
arduino-cli compile --profile uno_r4_wifi --build-path build/wifi firmware/TallyLight
arduino-cli upload --profile uno_r4_wifi --port COM6 --build-path build/wifi firmware/TallyLight
```

Run upload only after compilation succeeds. Unlike the IDE's Upload button, the CLI's
[`upload` command](https://arduino.github.io/arduino-cli/1.5/commands/arduino-cli_upload/)
does **not compile**. Specify the build directory that matches the latest successful build:
`build/wifi` for `tools/build.ps1`, or `build/usb` for `tools/build.ps1 -UsbOnly`.

For a checkout with the optional local CLI/config installed, this PowerShell sequence builds
and uploads the USB-only variant from the repository root, stopping if compilation fails:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1 -UsbOnly
if ($LASTEXITCODE -eq 0) {
    & .\.tools\arduino-cli\arduino-cli.exe --config-file .\.tools\arduino-cli.yaml upload --profile uno_r4_wifi --port COM5 --build-path .\build\usb .\firmware\TallyLight
}
```

Use your current port in place of `COM5`/`COM6`. Close Serial Monitor or any other program
holding the port if upload reports access denied. `-UsbOnly` disables Wi-Fi; a normal build
also supports USB, so it is not required just because commands arrive over USB.
For the Wi-Fi-capable variant, omit `-UsbOnly` and upload from `build/wifi`.

Upload is a separate, deliberate step; building and running tests never flash the board.
`sketch.yaml` pins the core; WiFiS3 comes with it and
there are no third-party library dependencies. `-FactoryDefaults` ignores `config.local.h`
for build validation; edits to `config.h` still apply. See [development notes](docs/development.md)
for Windows path limits.

## Windows commands and Stream Deck

```powershell
./tools/set-tally-mode.ps1 -Mode hot
./tools/set-tally-mode.ps1 -Mode off -Port COM6
./tools/set-tally-mode.ps1 -Mode idle -Port COM6 -PassThru
./tools/set-tally-mode.ps1 -Mode status -Port COM6
```

Automatic discovery selects a connected UNO R4 WiFi. If more than one is connected, specify
`-Port`. Commands are case-insensitive. Success is silent unless `-PassThru` or a status
query is used. Failures terminate with an error; `powershell.exe -File` returns a nonzero exit.
The client waits for a matching acknowledgement and retries at most twice within its
default three-second deadline. It does **not** wait three seconds on successful commands.
Opening the port and starting PowerShell still add host-side latency.

For a hidden hotkey/Stream Deck action, use `powershell.exe` with arguments:

```text
-NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -File "C:\path\ArduinoMicTallyLight\tools\set-tally-mode.ps1" -Mode hot -Port COM6
```

The original `PowerShell commands/set-tally-mode.ps1` path forwards to the new client, so
existing shortcuts still work after the firmware upload. Close Arduino Serial Monitor
before another program opens the same port. For the lowest host overhead, keep a serial
connection open and send commands directly using the [USB protocol](docs/protocol.md).

## Wi-Fi API

Find the board's address using the USB `status` command (`ip` and `wifi` fields).

```powershell
./tools/set-tally-mode.ps1 -Mode hot -DeviceUrl http://192.168.1.42
./tools/set-tally-mode.ps1 -Mode status -DeviceUrl http://192.168.1.42
```

The API retains `GET /status` and `POST /tally` with `{"mode":"hot"}`. POST requests must now
use `Content-Type: application/json` and `Content-Length`. `GET /` also returns status JSON.
Responses report the actual shared state; malformed requests return errors without changing it.

Set `TALLY_HTTP_TOKEN` in the local configuration to require `Authorization: Bearer <token>`
on all HTTP endpoints; pass `-Token` in the client. An empty token allows LAN control.
HTTP is unencrypted: use it on a trusted local network. There is no wildcard CORS or public
Internet service. See the [protocol reference](docs/protocol.md) for limits and status fields.

## Hardware and testing

Existing channel B wiring is retained: **DIR D13, PWM D11, BRAKE D8**. Default polarity is
LOW for red and HIGH for white. Set `TALLY_RED_DIRECTION_HIGH` if your wiring is reversed.
The bridge is disabled before reversing polarity. The PWM carrier remains approximately
490 Hz; the 100 Hz value above is the animation update rate.

The known setup uses an unmodified shield and a nominal 12 V, 1 A adapter. No jumper or
supply change is required by this rewrite. Electrical headroom and the light's response
to PWM still need a physical check; 12-bit brightness cannot by itself fix an undervoltage
or minimum-pulse problem. See [hardware notes and the bench procedure](docs/hardware.md).

```powershell
./tools/test-native.cmd       # Visual Studio C++ Build Tools
powershell.exe -NoProfile -File tests/test-command.ps1
```

```sh
bash tools/test.sh            # GCC/Clang, AddressSanitizer and UndefinedBehaviorSanitizer
pwsh -NoProfile -File tests/test-command.ps1
```

Tests cover all mode transitions, exact legacy curve reconstruction, high-resolution
animation, `millis()` wrap, fragmented/malformed requests, acknowledgements, backpressure,
missing Wi-Fi/modem, reconnects, DHCP waits, and bridge polarity sequencing. GitHub Actions
runs host tests and compiles Wi-Fi and USB-only firmware. Simulation does not establish
physical brightness, PWM timing or Wi-Fi latency on the actual board.

The previous firmware is preserved in [`Archive/`](Archive/). The original notebooks and
spreadsheet remain in `Lookup table calculation/` as historical research; they are not
build inputs. The Gaussian derivation follows [Alexandra's two-piece distribution work](https://github.com/quantgirluk/twopiece).
MIT license; original project copyright is retained in [LICENSE](LICENSE).
