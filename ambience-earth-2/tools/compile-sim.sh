#!/bin/bash
set -euo pipefail

date
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3 \
  --build-path main/build \
  --jobs 0 \
  main
date
