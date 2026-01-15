#!/bin/bash
set -euo pipefail

project_name="$(basename "$PWD")"
sketch_file="${project_name}.ino"

date
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3 \
  --build-path build \
  --jobs 0 \
  .
date
