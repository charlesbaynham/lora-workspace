# lora-workspace

PlatformIO firmware for two Heltec WiFi LoRa 32 **V4** boards (ESP32-S3 + SX1262).

The LoRa link between the two boards works; see [`docs/lora-bringup.md`](docs/lora-bringup.md).
Hardware reference (pins, GNSS wiring, PA control, schematics): [`docs/`](docs/README.md).

## Boards

| Board MAC (USB serial no.) | GNSS module | Firmware env |
|---|---|---|
| `90:DA:72:FB:75:FC` | **attached** (NMEA, 9600 baud, GPIO39) | `lora_link` |
| `90:DA:72:FE:0A:AC` | none | `lora_link` |

Both are V4.3 (KCT8103L PA). The link between them works symmetrically
(about -12 dBm RSSI at 30 cm, +10 dBm TX). A poorly seated antenna had caused a
46 dB asymmetry earlier; if a link looks lopsided, reseat the antennas first. See
[`docs/lora-bringup.md`](docs/lora-bringup.md#the-46-db-asymmetry-poorly-seated-antenna).

`/dev/ttyACMn` numbers are not stable; use `/dev/serial/by-id/...` (see `scripts/ports.sh`).
`scripts/serial_capture.py PORT [SECONDS]` resets a board and prints its serial output.

## Build / flash / monitor

There is one firmware, `lora_link`, and both boards run it. It includes the LoRa link, the OLED, Bluetooth for the
phone app, and updates over Bluetooth and Wi-Fi.

```fish
set -x PLATFORMIO_CORE_DIR $PWD/.platformio       # keep toolchains inside the repo
uvx --from platformio pio run                     # build
uvx --from platformio pio run -t upload --upload-port <port>
uv run --with pyserial python -I scripts/serial_capture.py <port> 30
```

A global `pio` works the same way. Two identical boards are attached, so the
upload port is deliberately not pinned in `platformio.ini`. `scripts/ports.sh`
lists them; use the `/dev/serial/by-id/...` path.

## Screen

The firmware shows the link state, RSSI/SNR (both directions), noise floor, counters and an RSSI chart
on the board's OLED, on four pages that rotate every 5 s (PRG button: next page), the fourth with the Wi-Fi state
(saved network, last join, update progress). See
[`docs/oled-display.md`](docs/oled-display.md#what-the-link-firmware-shows).

## Phone web app (BLE)

Each board advertises over BLE as `LoRa-<id>`. `web/index.html` is a single static file that connects
with Web Bluetooth to either board and shows whether the two boards are linked
(*Linked* / *One-way* / *Link down*), the RSSI and SNR each board hears from the other,
packet loss, noise floor, counters, a chart and a packet log. How the link state is
worked out: [`docs/lora-bringup.md`](docs/lora-bringup.md#link-state-and-the-beacon).

Web Bluetooth needs **Chrome on Android** and a **secure context** (HTTPS, or `localhost`).
Serve it on plain HTTP and let the reverse proxy add TLS. For a quick local run:

```fish
python3 -m http.server 8080 -d web
```

Quick test without a proxy: `adb reverse tcp:8080 tcp:8080`, then open `http://localhost:8080` in Chrome on the phone.
The GATT UUIDs are defined in both `src/common/ble_link.h` and `web/index.html`; keep them in sync.

## Firmware updates from the phone

The firmware can be updated over BLE from the same web app (no USB): connect to a board and
tap *Update firmware*. The app installs the image the web server builds from the latest `master`
(`scripts/build_release.sh`, run on the server for each push; images are not committed). Pairing passkey: `123456` (change it with `-DOTA_PASSKEY=...`).
Details, protocol and limits: [`docs/ble-ota.md`](docs/ble-ota.md). A board that does not yet run this firmware needs one USB upload first.

At home, the same button row has *Update over Wi-Fi*, which is much faster: pick the Wi-Fi network once
in the page's Wi-Fi card (scan, choose, password; the board keeps it in its own flash, across updates
and USB uploads; nothing is in the repository or the image), then the board downloads the published image
itself. See [`docs/wifi-ota.md`](docs/wifi-ota.md).

## One-time: serial permissions

The boards enumerate as `root:dialout 0660`. Either join the group (needs re-login):

```fish
sudo usermod -aG dialout $USER
```

or install the udev rule in this repo (no re-login; replug the boards):

```fish
sudo cp udev/99-heltec-esp32s3.rules /etc/udev/rules.d/
sudo udevadm control --reload; and sudo udevadm trigger
```

## Layout

- `src/lora_link/`: the firmware, a bidirectional LoRa link (both boards run it)
- `src/common/heltec_v4.h`: pins and front-end control
- `src/common/ble_link.h`: BLE GATT feed for the phone web app
- `src/common/ble_ota.h`: over-the-air firmware update over BLE ([`docs/ble-ota.md`](docs/ble-ota.md))
- `src/common/ble_wifi.h`: Wi-Fi network setup and firmware update over Wi-Fi ([`docs/wifi-ota.md`](docs/wifi-ota.md))
- `scripts/build_release.sh`: builds `lora_link` into a release directory (image + manifest) for the web app; run by the server on each push
- `web/`: phone web app (Web Bluetooth) and its systemd unit, see below
- `docs/`: hardware and radio reference, Heltec's datasheets and schematics
