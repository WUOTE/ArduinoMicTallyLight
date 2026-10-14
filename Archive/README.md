# Historical firmware

These files preserve the original Wi-Fi and Arduino Cloud implementations for reference.
The maintained sketch is now `../firmware/TallyLight/TallyLight.ino`.
The legacy Wi-Fi version is the source of the curve regression fixture in `../tests/fixtures/`.

The archived Cloud sketch used the opposite red/white polarity from the later Wi-Fi sketch.
The rewrite retains the later Wi-Fi sketch's channel B pins and polarity.
Do not copy the Cloud pin logic into the new driver without checking the wiring.

Credentials have been moved to the ignored `firmware/TallyLight/arduino_secrets.h` in the local
working copy. They are no longer part of the current source tree's tracked configuration.
This does not remove credentials from earlier Git commits.
