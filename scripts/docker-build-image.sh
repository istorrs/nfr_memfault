#!/usr/bin/env bash
set -euo pipefail

NCS_VERSION="${NCS_VERSION:-v3.3.0}"
IMAGE="${IMAGE:-nfr-memfault-ncs:${NCS_VERSION}}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

docker build \
  --build-arg "NCS_VERSION=${NCS_VERSION}" \
  -t "${IMAGE}" \
  "${ROOT_DIR}/docker"

echo "Built ${IMAGE}"

