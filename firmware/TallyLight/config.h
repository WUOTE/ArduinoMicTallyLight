#pragma once

// Optional credentials, ignored by Git. A clean checkout works over USB.
#if !defined(TALLY_NO_LOCAL_CONFIG) && __has_include("config.local.h")
#include "config.local.h"
#endif

// EDIT BREATHING HERE. Times are milliseconds (1000 = 1 second).
// The cycle is rise + peak hold + fall + off hold, calculated automatically.
// Changing a hold extends the cycle without speeding up either fade.
#ifndef TALLY_RISE_MS
#define TALLY_RISE_MS 1680U
#endif
#ifndef TALLY_PEAK_HOLD_MS
#define TALLY_PEAK_HOLD_MS 400U
#endif
#ifndef TALLY_FALL_MS
#define TALLY_FALL_MS 3120U
#endif
#ifndef TALLY_FLOOR_HOLD_MS
#define TALLY_FLOOR_HOLD_MS 800U  // Time off when the floor is zero.
#endif
#ifndef TALLY_TRANSITION_MS
#define TALLY_TRANSITION_MS 250U  // Time to join a new mode after a command.
#endif
#ifndef TALLY_RED_FLOOR
#define TALLY_RED_FLOOR 0.0f
#endif
#ifndef TALLY_WHITE_FLOOR
#define TALLY_WHITE_FLOOR 0.0f
#endif

// Keep Wi-Fi credentials in config.local.h; edit light settings above.
#ifndef TALLY_WIFI_SSID
#define TALLY_WIFI_SSID ""
#endif
#ifndef TALLY_WIFI_PASSWORD
#define TALLY_WIFI_PASSWORD ""
#endif
#ifndef TALLY_HTTP_TOKEN
#define TALLY_HTTP_TOKEN ""
#endif
#ifndef TALLY_ENABLE_WIFI
#define TALLY_ENABLE_WIFI 1
#endif
#ifdef TALLY_USB_ONLY
#undef TALLY_ENABLE_WIFI
#define TALLY_ENABLE_WIFI 0
#endif
#ifndef TALLY_RED_DIRECTION_HIGH
#define TALLY_RED_DIRECTION_HIGH 0
#endif
#ifndef TALLY_MODEM_TIMEOUT_MS
#define TALLY_MODEM_TIMEOUT_MS 50U
#endif

static_assert(TALLY_RED_FLOOR >= 0 && TALLY_RED_FLOOR <= 1, "Red floor must be 0..1");
static_assert(TALLY_WHITE_FLOOR >= 0 && TALLY_WHITE_FLOOR <= 1, "White floor must be 0..1");
static_assert(TALLY_RISE_MS >= 1 && TALLY_RISE_MS <= 600000,
              "Rise must be 1..600000 ms");
static_assert(TALLY_FALL_MS >= 1 && TALLY_FALL_MS <= 600000,
              "Fall must be 1..600000 ms");
static_assert(TALLY_PEAK_HOLD_MS >= 0 && TALLY_PEAK_HOLD_MS <= 600000,
              "Peak hold must be 0..600000 ms");
static_assert(TALLY_FLOOR_HOLD_MS >= 0 && TALLY_FLOOR_HOLD_MS <= 600000,
              "Floor hold must be 0..600000 ms");
static_assert(TALLY_TRANSITION_MS >= 10 && TALLY_TRANSITION_MS <= 5000,
              "Transition must be 10..5000 ms");
static_assert(TALLY_MODEM_TIMEOUT_MS >= 10 && TALLY_MODEM_TIMEOUT_MS <= 250,
              "Modem timeout must be 10..250 ms");
