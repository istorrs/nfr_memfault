ARG NCS_VERSION=v3.3.0
FROM ghcr.io/nrfconnect/sdk-nrf-toolchain:${NCS_VERSION}

SHELL ["/bin/bash", "-lc"]

WORKDIR /work

# Keep the image thin. The Nordic image already carries the NCS toolchain,
# Python environment, west, CMake, Ninja, and Nordic command-line tooling.
ENV ZEPHYR_BASE=/work/zephyr

CMD ["bash"]

