#pragma once

// Optional, ignored by Git. A clean checkout works over USB without credentials.
#if !defined(TALLY_NO_LOCAL_CONFIG) && __has_include("config.local.h")
#include "config.local.h"
#endif

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
#ifndef TALLY_RED_FLOOR
#define TALLY_RED_FLOOR 0.0f
#endif
#ifndef TALLY_WHITE_FLOOR
#define TALLY_WHITE_FLOOR 0.0f
#endif
#ifndef TALLY_BREATH_PERIOD_MS
#define TALLY_BREATH_PERIOD_MS 4800U
#endif
#ifndef TALLY_TRANSITION_MS
#define TALLY_TRANSITION_MS 250U
#endif
#ifndef TALLY_MODEM_TIMEOUT_MS
#define TALLY_MODEM_TIMEOUT_MS 50U
#endif

static_assert(TALLY_RED_FLOOR >= 0 && TALLY_RED_FLOOR <= 1, "Red floor must be 0..1");
static_assert(TALLY_WHITE_FLOOR >= 0 && TALLY_WHITE_FLOOR <= 1, "White floor must be 0..1");
static_assert(TALLY_BREATH_PERIOD_MS >= 100 && TALLY_BREATH_PERIOD_MS <= 600000,
              "Breath period must be 100..600000 ms");
static_assert(TALLY_TRANSITION_MS >= 10 && TALLY_TRANSITION_MS <= 5000,
              "Transition must be 10..5000 ms");
static_assert(TALLY_MODEM_TIMEOUT_MS >= 10 && TALLY_MODEM_TIMEOUT_MS <= 250,
              "Modem timeout must be 10..250 ms");
