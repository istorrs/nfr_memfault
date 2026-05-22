#!/usr/bin/env bash
set -euo pipefail

NCS_VERSION="${NCS_VERSION:-v3.3.0}"
IMAGE="${IMAGE:-nfr-memfault-ncs:${NCS_VERSION}}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

BUILD_DIR="${1:-build/memfault_upload}"
SERIAL="${2:-${JLINK_SERIAL:-}}"

FLASH_ARGS=(west flash -d "${BUILD_DIR}")
if [[ -n "${SERIAL}" ]]; then
  FLASH_ARGS+=(--dev-id "${SERIAL}")
fi

mkdir -p "${ROOT_DIR}/.home"

docker run --rm \
  --entrypoint /bin/bash \
  --privileged \
  -e HOME=/work/.home \
  -v /dev/bus/usb:/dev/bus/usb \
  -v /run/udev:/run/udev:ro \
  -v "${ROOT_DIR}:/work" \
  -w /work \
  "${IMAGE}" \
  -lc "$(printf '%q ' "${FLASH_ARGS[@]}")"
