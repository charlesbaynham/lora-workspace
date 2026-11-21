# lora-workspace

PlatformIO firmware for two Heltec WiFi LoRa 32 **V4** boards (ESP32-S3 + SX1262).

## Build / flash / monitor

    pio run                                   # build
    pio run -t upload  --upload-port <port>   # flash one board
    pio device monitor --port <port>          # serial monitor

No global `pio`? `uvx --from platformio pio ...` works the same; add
`PLATFORMIO_CORE_DIR=$PWD/.platformio` to keep all toolchains inside the repo.

Two identical boards are attached, so the upload port is deliberately not pinned
in `platformio.ini`. `scripts/ports.sh` lists them. Use the stable
`/dev/serial/by-id/...` path (the serial number *is* the board's MAC), not
`/dev/ttyACMn`, whose numbering changes between plugs.

## One-time: serial permissions

The boards enumerate as `root:dialout 0660`. Either join the group (needs re-login):

    sudo usermod -aG dialout $USER

or install the udev rule in this repo (no re-login; replug the boards):

    sudo cp udev/99-heltec-esp32s3.rules /etc/udev/rules.d/
    sudo udevadm control --reload && sudo udevadm trigger

## Open questions before real radio work

- The V4 has a GC1109 PA/front-end. Its enable/TX-select GPIOs are **not** in the
  Arduino variant file, so `src/main.cpp` does not drive them; transmitting without
  them will be weak or silent. Confirm pins against Heltec's V4 schematic/examples.
- Frequency is set to 868 MHz (EU868). Check duty-cycle/power limits before TX.
