#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Ifirmware/TallyLight/src tests/test_core.cpp \
  firmware/TallyLight/src/controller.cpp firmware/TallyLight/src/protocol.cpp \
  -o build/tests/core-tests
build/tests/core-tests
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -pedantic -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -include tests/stubs/test_config.h -Itests/stubs -Ifirmware/TallyLight/src \
  tests/test_firmware.cpp tests/stubs/hardware.cpp firmware/TallyLight/src/controller.cpp \
  firmware/TallyLight/src/protocol.cpp firmware/TallyLight/src/firmware.cpp \
  -o build/tests/firmware-tests
for scenario in online offline missing-modem; do
  build/tests/firmware-tests "$scenario"
done
