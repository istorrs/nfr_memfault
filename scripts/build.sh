#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCAL_CONFIG="${LOCAL_CONFIG:-${ROOT_DIR}/.build.env}"

ENV_NCS_VERSION="${NCS_VERSION-}"
ENV_IMAGE="${IMAGE-}"
ENV_APP="${APP-}"
ENV_BOARD="${BOARD-}"
ENV_BUILD_DIR="${BUILD_DIR-}"
ENV_MEMFAULT_PROJECT_KEY="${MEMFAULT_PROJECT_KEY-}"
ENV_DEVICE_ID="${DEVICE_ID-}"
ENV_BT_DEVICE_NAME="${BT_DEVICE_NAME-}"

if [[ -f "${LOCAL_CONFIG}" ]]; then
  # shellcheck source=/dev/null
  source "${LOCAL_CONFIG}"
fi

NCS_VERSION="${ENV_NCS_VERSION:-${NCS_VERSION:-v3.3.0}}"
IMAGE="${ENV_IMAGE:-${IMAGE:-nfr-memfault-ncs:${NCS_VERSION}}}"
APP="${ENV_APP:-${APP:-apps/memfault_upload}}"
APP_NAME="$(basename "${APP}")"
BOARD="${ENV_BOARD:-${BOARD:-nrf52840dk/nrf52840}}"
BUILD_DIR="${ENV_BUILD_DIR:-${BUILD_DIR:-build/${APP_NAME}}}"
MEMFAULT_PROJECT_KEY="${ENV_MEMFAULT_PROJECT_KEY:-${MEMFAULT_PROJECT_KEY:-}}"
DEVICE_ID="${ENV_DEVICE_ID:-${DEVICE_ID:-}}"
BT_DEVICE_NAME="${ENV_BT_DEVICE_NAME:-${BT_DEVICE_NAME:-}}"

: "${MEMFAULT_PROJECT_KEY:?Set MEMFAULT_PROJECT_KEY before building}"

die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

ensure_docker() {
  if ! command -v docker >/dev/null 2>&1; then
    die "Docker is required but was not found in PATH."
  fi

  if ! docker info >/dev/null 2>&1; then
    die "Docker is not running or the current user cannot access it."
  fi
}

ensure_docker_image() {
  if docker image inspect "${IMAGE}" >/dev/null 2>&1; then
    return
  fi

  printf 'Docker image %s is missing; building it now.\n' "${IMAGE}" >&2
  NCS_VERSION="${NCS_VERSION}" IMAGE="${IMAGE}" "${ROOT_DIR}/scripts/docker-build-image.sh"
}

workspace_ready() {
  [[ -f "${ROOT_DIR}/.west/config" && -d "${ROOT_DIR}/nrf" && -d "${ROOT_DIR}/zephyr" ]]
}

ensure_west_workspace() {
  if workspace_ready; then
    return
  fi

  cat >&2 <<EOF
This workspace is missing the nRF Connect SDK checkout.

Running ./scripts/west-init.sh now. This can take a while on the first run.
EOF

  NCS_VERSION="${NCS_VERSION}" IMAGE="${IMAGE}" "${ROOT_DIR}/scripts/west-init.sh"

  if ! workspace_ready; then
    die "west initialization finished, but .west/config, nrf/, or zephyr/ is still missing."
  fi
}

ensure_docker
ensure_docker_image
ensure_west_workspace

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
