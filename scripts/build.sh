#!/usr/bin/env bash
set -euo pipefail

NCS_VERSION="${NCS_VERSION:-v3.3.0}"
IMAGE="${IMAGE:-nfr-memfault-ncs:${NCS_VERSION}}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

APP="${APP:-apps/memfault_upload}"
APP_NAME="$(basename "${APP}")"
BOARD="${BOARD:-nrf52840dk/nrf52840}"
BUILD_DIR="${BUILD_DIR:-build/${APP_NAME}}"

: "${MEMFAULT_PROJECT_KEY:?Set MEMFAULT_PROJECT_KEY before building}"

mkdir -p "${ROOT_DIR}/.build-overlays" "${ROOT_DIR}/.ccache" "${ROOT_DIR}/.home"
OVERLAY_CONF="${ROOT_DIR}/.build-overlays/${APP_NAME}.conf"

{
  printf 'CONFIG_MEMFAULT_NCS_PROJECT_KEY="%s"\n' "${MEMFAULT_PROJECT_KEY}"
  printf 'CONFIG_MEMFAULT_NCS_DEVICE_ID="%s"\n' "${DEVICE_ID:-nrf52840-memfault-ble}"
  printf 'CONFIG_BT_DEVICE_NAME="%s"\n' "${BT_DEVICE_NAME:-NFR_Memfault}"
} > "${OVERLAY_CONF}"

WEST_ARGS=(
  west build -p always
  -b "${BOARD}"
  -d "${BUILD_DIR}"
  "${APP}"
  --
  -DEXTRA_CONF_FILE="/work/.build-overlays/${APP_NAME}.conf"
)

docker run --rm \
  --entrypoint /bin/bash \
  -u "$(id -u):$(id -g)" \
  -e HOME=/work/.home \
  -e CCACHE_DIR=/work/.ccache \
  -v "${ROOT_DIR}:/work" \
  -w /work \
  "${IMAGE}" \
  -lc "$(printf '%q ' "${WEST_ARGS[@]}")"
