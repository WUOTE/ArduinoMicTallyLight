## About

Control and drive your Yellowtec Mika Mic Tally Light using an Arduino and Adafruit Motor Shield via Stream Deck.
This project was inspired by excellent [Avital Pekker's blogpost](https://avital.ca/notes/a-closer-look-at-apples-breathing-light) and work done by [Quant Girl's on two-piece normal distribution](https://quantgirl.blog/two-piece-normal/) implementation in python.

## Preparation

1. Fill in your WiFi network name and password in `arduino_secrets.h`
1. Upload the sketch `MicTallyLightControlWiFi.ino` file to your Arduino UNO R4 WiFi.

## Usage

The tally light is controlled by the `set-tally-mode.ps1` script located in the `PowerShell commands` directory. This script automatically finds the correct COM port for the Arduino.

You can run it from a PowerShell terminal directly, or you can configure your Stream Deck to call it using terminal for a silent, invisible execution.

### Example Commands for Stream Deck or Shortcuts

Here is how you can configure Stream Deck buttons to run the script silently. You must use the full, absolute path to the `set-tally-mode.ps1` script.

**Set mode to `off`:**

```powershell
powershell -NoExit -ExecutionPolicy Bypass -File "C:\path_to_your_script\set-tally-mode.ps1" -Mode off
```

**Set mode to `hot` (breathing red):**

```powershell
powershell -NoExit -ExecutionPolicy Bypass -File "C:\path_to_your_script\set-tally-mode.ps1" -Mode hot
```

**Set mode to `idle` (breathing white):**

```powershell
powershell -NoExit -ExecutionPolicy Bypass -File "C:\path_to_your_script\set-tally-mode.ps1" -Mode idle
```

**Set mode to `red` (solid red color at full brightness):**

```powershell
powershell -NoExit -ExecutionPolicy Bypass -File "C:\path_to_your_script\set-tally-mode.ps1" -Mode red
```

**Set mode to `white` (solid white color at full brightness):**

```powershell
powershell -NoExit -ExecutionPolicy Bypass -File "C:\path_to_your_script\set-tally-mode.ps1" -Mode white
```

## How to make your own

Feel free to look at all the supporting files, I've tried to document and explain every decision with links to sources and authors.

## How to contribute

If you spot an error or have an idea how to improve the existing solution feel free to open a PR.
