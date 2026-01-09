#!/bin/bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
chip_dir="${repo_root}/eeprom-chip"
image_tag="ambience-earth-wokwi-chips"

if command -v clang >/dev/null 2>&1 && command -v wasm-ld >/dev/null 2>&1; then
  make -C "${chip_dir}"
  exit 0
fi

if command -v devcontainer >/dev/null 2>&1; then
  devcontainer up --workspace-folder "${chip_dir}" --skip-post-create
  devcontainer exec --workspace-folder "${chip_dir}" make
  exit 0
fi

if ! command -v docker >/dev/null 2>&1; then
  echo "Missing docker/devcontainer; install one or run make inside the eeprom-chip dev container." >&2
  exit 1
fi

if ! docker image inspect "${image_tag}" >/dev/null 2>&1; then
  docker build -t "${image_tag}" -f "${chip_dir}/.devcontainer/Dockerfile" "${chip_dir}/.devcontainer"
fi

docker run --rm -v "${chip_dir}:/workspaces/eeprom-chip" -w /workspaces/eeprom-chip "${image_tag}" make
