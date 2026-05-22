#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCAL_CONFIG="${LOCAL_CONFIG:-${ROOT_DIR}/.build.env}"

ENV_NCS_VERSION="${NCS_VERSION-}"
ENV_IMAGE="${IMAGE-}"

if [[ -f "${LOCAL_CONFIG}" ]]; then
  # shellcheck source=/dev/null
  source "${LOCAL_CONFIG}"
fi

NCS_VERSION="${ENV_NCS_VERSION:-${NCS_VERSION:-v3.3.0}}"
IMAGE="${ENV_IMAGE:-${IMAGE:-nfr-memfault-ncs:${NCS_VERSION}}}"

: "${JLINK_SENSOR:?Set JLINK_SENSOR in .build.env}"
: "${JLINK_GATEWAY:?Set JLINK_GATEWAY in .build.env}"

SENSOR_BUILD_DIR="${SENSOR_BUILD_DIR:-build/sensor}"
GATEWAY_BUILD_DIR="${GATEWAY_BUILD_DIR:-build/gateway}"

mkdir -p "${ROOT_DIR}/.home"

JLINK_DIR="${JLINK_DIR:-/opt/SEGGER/JLink_V944}"

flash_device() {
  local serial="$1"
  local label="$2"
  local build_dir="$3"
  local flash_args=(west flash -d "${build_dir}" --runner jlink --dev-id "${serial}")
  echo "[${label}] Flashing ${serial}..."
  docker run --rm \
    --entrypoint /bin/bash \
    --privileged \
    -e HOME=/work/.home \
    -e GIT_CONFIG_COUNT=1 \
    -e GIT_CONFIG_KEY_0=safe.directory \
    -e GIT_CONFIG_VALUE_0='*' \
    -v /dev/bus/usb:/dev/bus/usb \
    -v /run/udev:/run/udev:ro \
    -v "${JLINK_DIR}:${JLINK_DIR}" \
    -v "${ROOT_DIR}:/work" \
    -w /work \
    "${IMAGE}" \
    -lc "export PATH=${JLINK_DIR}:\$PATH && export LD_LIBRARY_PATH=${JLINK_DIR}:\${LD_LIBRARY_PATH:-} && $(printf '%q ' "${flash_args[@]}")" \
    && echo "[${label}] Flash complete" \
    || echo "[${label}] Flash FAILED"
}

flash_device "${JLINK_SENSOR}"  "sensor"  "${SENSOR_BUILD_DIR}"  &
flash_device "${JLINK_GATEWAY}" "gateway" "${GATEWAY_BUILD_DIR}" &
wait
