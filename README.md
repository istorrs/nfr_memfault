# nRF52840 Memfault BLE Gateway Skeleton

This repository is a headless nRF Connect SDK workspace skeleton for an
nRF52840 DK running Memfault over Bluetooth Low Energy. The application exposes
Nordic's Memfault Diagnostic Service (MDS); an nRF Connect BLE gateway, such as
nRF Connect Device Manager on a phone, bonds to the DK, reads Memfault chunks
over BLE, and forwards them to Memfault Cloud.

The build flow is Docker-only. No VS Code or host-side Nordic toolchain install
is required.

## Requirements

- Docker
- nRF52840 DK
- J-Link USB access for flashing from Docker
- Memfault project key
- nRF Connect Device Manager or another BLE gateway that supports Memfault MDS

No nRF7002 EK/shield is required for this baseline.

## One-time setup

```sh
./scripts/docker-build-image.sh
./scripts/west-init.sh
```

`west-init.sh` downloads the nRF Connect SDK modules into this workspace.

## Build

```sh
MEMFAULT_PROJECT_KEY='your-memfault-project-key' ./scripts/build.sh
```

The default target is:

- board: `nrf52840dk/nrf52840`
- app: `apps/memfault_upload`
- BLE name: `NFR_Memfault`

Override build-time identity when needed:

```sh
MEMFAULT_PROJECT_KEY='...' DEVICE_ID='sensor-001' BT_DEVICE_NAME='NFR_Sensor_001' ./scripts/build.sh
```

## Flash

```sh
./scripts/flash.sh
```

If two DKs are connected, pass a J-Link serial number:

```sh
./scripts/flash.sh build/memfault_upload 1050123456
```

The flash script runs Docker with USB access and calls `west flash` inside the
container.

## Runtime

On boot the app:

1. initializes Bluetooth,
2. advertises the Memfault Diagnostic Service UUID,
3. accepts pairing/bonding from the BLE gateway,
4. enables MDS access for the bonded peer,
5. lets the gateway collect Memfault chunks and forward them to Memfault Cloud.

The nRF52840 firmware does not upload directly to Memfault Cloud in this mode.
The cloud transport runs on the gateway, which keeps Wi-Fi, TLS, DNS, and socket
RAM out of the device firmware.

The serial shell is enabled. Useful commands include:

```text
mflt help
kernel reboot cold
```

Use the nRF Connect Device Manager app's Memfault or diagnostic upload flow to
collect and forward chunks over BLE.

## Notes

- Wi-Fi, networking, TLS, certificate provisioning, and the Memfault HTTP uploader
  are intentionally disabled in this baseline.
- The Memfault project key is injected by the build script into
  `.build-overlays/`; it is not committed.
- The app directory is still named `memfault_upload` for continuity, but the
  current transport is BLE MDS gateway upload, not direct device HTTP upload.
- A verified dummy build used about 254 KB flash and 55 KB RAM on
  `nrf52840dk/nrf52840`.
- The planned next step is splitting this into `sensor` and `gateway`
  applications. With only nRF52840 DKs, both sides can use BLE locally, while a
  phone or Linux BLE gateway handles internet upload to Memfault Cloud.
