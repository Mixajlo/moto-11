#!/usr/bin/env bash
# Native (host) unit tests for the engine-run supervisor. Run under WSL/Linux:
#   bash test_host/run.sh
# Needs g++ only (sudo apt install build-essential). No ESP / PlatformIO.
set -euo pipefail
cd "$(dirname "$0")/.."          # -> firmware/

TMP="$(mktemp -d)"

g++ -std=c++17 -DMOTO_DEBUG=1 \
    -I test_host/mock -I src \
    test_host/test_supervisor_native.cpp \
    src/supervisor.cpp src/sensors.cpp src/relays.cpp \
    -o "$TMP/moto_sup_test"
"$TMP/moto_sup_test"
echo "native supervisor tests: PASS"

g++ -std=c++17 -DMOTO_DEBUG=1 \
    -I test_host/mock -I src \
    test_host/test_config_native.cpp \
    src/config.cpp src/supervisor.cpp src/sensors.cpp src/relays.cpp src/loads.cpp src/motion.cpp \
    -o "$TMP/moto_cfg_test"
"$TMP/moto_cfg_test"
echo "native config tests: PASS"

g++ -std=c++17 -DMOCK_PINS -DMOCK_NVS \
    -I test_host/mock -I src \
    test_host/test_buttons_native.cpp \
    src/buttons.cpp src/relays.cpp src/loads.cpp \
    -o "$TMP/moto_btn_test"
"$TMP/moto_btn_test"
echo "native button tests: PASS"

g++ -std=c++17 -DMOTO_DEBUG=1 \
    -I test_host/mock -I src \
    test_host/test_loads_native.cpp \
    src/loads.cpp src/relays.cpp \
    -o "$TMP/moto_loads_test"
"$TMP/moto_loads_test"
echo "native load tests: PASS"

g++ -std=c++17 -DMOTO_DEBUG=1 \
    -I test_host/mock -I src \
    test_host/test_motion_native.cpp \
    src/motion.cpp \
    -o "$TMP/moto_motion_test"
"$TMP/moto_motion_test"
echo "native motion tests: PASS"
