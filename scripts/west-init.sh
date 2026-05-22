#!/usr/bin/env bash
set -euo pipefail

NCS_VERSION="${NCS_VERSION:-v3.3.0}"
IMAGE="${IMAGE:-nfr-memfault-ncs:${NCS_VERSION}}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mkdir -p "${ROOT_DIR}/manifest" "${ROOT_DIR}/.home"
WEST_INIT_CMD="if [ ! -d .west ]; then mkdir -p .west; printf '[manifest]\npath = manifest\nfile = west.yml\n' > .west/config; fi; west update"

docker run --rm \
  --entrypoint /bin/bash \
  -u "$(id -u):$(id -g)" \
  -e HOME=/work/.home \
  -v "${ROOT_DIR}:/work" \
  -w /work \
  "${IMAGE}" \
  -lc "${WEST_INIT_CMD}"
