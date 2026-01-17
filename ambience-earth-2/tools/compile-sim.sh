#!/bin/bash
set -euo pipefail

date
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3 \
  --build-path main/build \
  --build-property compiler.cpp.extra_flags="-DWOKWI_SIM" \
  --jobs 0 \
  main
date
