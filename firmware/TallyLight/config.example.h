#pragma once
// Copy to config.local.h in this folder. Do not commit real credentials.
#define TALLY_WIFI_SSID ""
#define TALLY_WIFI_PASSWORD ""
// Optional bearer token for HTTP control. USB needs no token.
#define TALLY_HTTP_TOKEN ""

// Motor Shield Rev3, channel B: DIR 13, PWM 11, BRAKE 8.
#define TALLY_RED_DIRECTION_HIGH 0
// Zero-floor breathing, tested on the current lamp. Raise if another lamp flickers near off.
#define TALLY_RED_FLOOR 0.0f
#define TALLY_WHITE_FLOOR 0.0f
#define TALLY_BREATH_PERIOD_MS 4800U
#define TALLY_TRANSITION_MS 250U

// Set to 0 for a USB-only build, with no modem initialization or Wi-Fi calls.
#define TALLY_ENABLE_WIFI 1
