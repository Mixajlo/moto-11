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
    src/config.cpp src/supervisor.cpp src/sensors.cpp src/relays.cpp \
    -o "$TMP/moto_cfg_test"
"$TMP/moto_cfg_test"
echo "native config tests: PASS"
