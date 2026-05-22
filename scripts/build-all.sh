#!/usr/bin/env bash
# Builds both apps sequentially. All .build.env variables apply to both;
# use SENSOR_DEVICE_ID / SENSOR_BT_NAME and GATEWAY_DEVICE_ID / GATEWAY_BT_NAME
# to differentiate the two devices.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
LOCAL_CONFIG="${LOCAL_CONFIG:-${ROOT_DIR}/.build.env}"

if [[ -f "${LOCAL_CONFIG}" ]]; then
  # shellcheck source=/dev/null
  source "${LOCAL_CONFIG}"
fi

APP=apps/sensor \
BUILD_DIR=build/sensor \
DEVICE_ID="${SENSOR_DEVICE_ID:-nrf52840-sensor}" \
BT_DEVICE_NAME="${SENSOR_BT_NAME:-NFR_Sensor}" \
  "${SCRIPT_DIR}/build.sh"

APP=apps/gateway \
BUILD_DIR=build/gateway \
DEVICE_ID="${GATEWAY_DEVICE_ID:-nrf52840-gateway}" \
BT_DEVICE_NAME="${GATEWAY_BT_NAME:-NFR_Gateway}" \
  "${SCRIPT_DIR}/build.sh"
