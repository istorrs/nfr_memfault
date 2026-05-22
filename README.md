# nRF52840 Memfault BLE Gateway Skeleton

This repository is an nRF Connect SDK workspace with two applications running on
nRF52840 DKs:

- **`apps/sensor`** — BLE peripheral that exposes the Memfault Diagnostic Service
  (MDS). A bonded BLE gateway reads Memfault chunks and forwards them to Memfault
  Cloud.
- **`apps/gateway`** — BLE central that scans for the sensor, bonds with it, and
  (TODO) forwards Memfault chunks to the cloud via the MDS GATT client.

The build flow is Docker-only. No VS Code or host-side Nordic toolchain install
is required.

## Requirements

- Docker
- Two nRF52840 DKs with J-Link USB access
- Memfault project key

## One-time setup

```sh
./scripts/docker-build-image.sh
./scripts/west-init.sh
```

## Build

Store local build settings in `.build.env` (gitignored):

```sh
MEMFAULT_PROJECT_KEY='your-memfault-project-key'
JLINK_SENSOR=001050203503
JLINK_GATEWAY=001050258833
```

Then build both apps:

```sh
./scripts/build-all.sh
```

To build a single app:

```sh
APP=apps/sensor BUILD_DIR=build/sensor DEVICE_ID=nrf52840-sensor BT_DEVICE_NAME=NFR_Sensor ./scripts/build.sh
APP=apps/gateway BUILD_DIR=build/gateway DEVICE_ID=nrf52840-gateway BT_DEVICE_NAME=NFR_Gateway ./scripts/build.sh
```

## Flash

```sh
./scripts/flash.sh
```

Flashes both devices in parallel using `JLINK_SENSOR` → `build/sensor` and
`JLINK_GATEWAY` → `build/gateway`. Both serials must be set in `.build.env`.

## Runtime

**Sensor** on boot:
1. Initializes Bluetooth and loads bonding state from flash.
2. Advertises the MDS service UUID as `NFR_Sensor`.
3. Accepts pairing/bonding from the gateway DK.
4. Enables MDS access for the bonded peer.

**Gateway** on boot:
1. Initializes Bluetooth and loads bonding state from flash.
2. Scans passively for a device advertising the MDS UUID.
3. Connects, bonds (`BT_SECURITY_L2`), and logs the connection.
4. MDS GATT client and chunk forwarding are TODO.

Both apps expose the **Zephyr shell** via the J-Link VCOM ports (ttyACM0/ttyACM2 on this host) — the default UART0 on P0.06 (TX) / P0.08 (RX) routed through the J-Link USB cable, 115200 8N1.
Useful shell commands:

```text
kernel uptime
kernel version
kernel threads
kernel reboot cold
mflt help
```

**RTT** is required for debug logging and GDB. Connect the J-Link USB cable (used
for flashing) and use a J-Link RTT viewer or `JLinkRTTLogger` to stream log output.
GDB attaches via the same J-Link connection using `west debug` inside the Docker
container.

## Notes

- Wi-Fi, networking, TLS, and the Memfault HTTP uploader are intentionally
  excluded. Cloud upload is delegated to the gateway (phone or DK).
- The Memfault project key is injected at build time into `.build-overlays/`
  and is never committed.
- `apps/memfault_upload` is retained as the original proof-of-concept baseline.

