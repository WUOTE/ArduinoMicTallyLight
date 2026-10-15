#pragma once
#define TALLY_NO_LOCAL_CONFIG
#define TALLY_WIFI_SSID "test-network"
#define TALLY_WIFI_PASSWORD "test-password"
#define TALLY_HTTP_TOKEN "test-token"
// A fixed fixture: changing the user's tuning must not alter integration expectations.
#define TALLY_RISE_MS 1680U
#define TALLY_PEAK_HOLD_MS 400U
#define TALLY_FALL_MS 3120U
#define TALLY_FLOOR_HOLD_MS 800U
#define TALLY_TRANSITION_MS 250U
#define TALLY_RED_FLOOR 0.0f
#define TALLY_WHITE_FLOOR 0.0f
