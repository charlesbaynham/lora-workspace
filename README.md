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

Environments (`platformio.ini`): `lora_link` (default), `smoke`, and experiments
(`lora_link_ble`, `lora_link_lna`, `lora_link_nognss[_lna]`, `lora_link_v42`, `fem_sweep`); see `docs/lora-bringup.md`.

```fish
set -x PLATFORMIO_CORE_DIR $PWD/.platformio       # keep toolchains inside the repo
uvx --from platformio pio run -e lora_link        # build
uvx --from platformio pio run -e lora_link -t upload --upload-port <port>
uv run --with pyserial python -I scripts/serial_capture.py <port> 30
```

A global `pio` works the same way. Two identical boards are attached, so the
upload port is deliberately not pinned in `platformio.ini`. `scripts/ports.sh`
lists them; use the `/dev/serial/by-id/...` path.

## Phone web app (BLE)

Flash the `lora_link_ble` env (`lora_link` plus a BLE feed; the board advertises as
`LoRa-<id>`). `web/index.html` is a single static file that connects with Web Bluetooth
and shows RSSI, SNR, noise floor, counters, a chart and a packet log.

Web Bluetooth needs **Chrome on Android** and a **secure context** (HTTPS, or `localhost`).
Serve it on plain HTTP and let the reverse proxy add TLS. For a quick local run:

```fish
python3 -m http.server 8080 -d web
```

Deploy to the Proxmox LXC (needs only `python3` in the container; replace `<ct>` with its
host or IP, and map port 8080 at the proxy):

```fish
ssh root@<ct> 'mkdir -p /opt/lora-web'
scp web/index.html root@<ct>:/opt/lora-web/
scp web/lora-web.service root@<ct>:/etc/systemd/system/
ssh root@<ct> 'systemctl daemon-reload && systemctl enable --now lora-web'
```

Updating the page later is just the `scp web/index.html` line. Logs: `journalctl -u lora-web`.

Quick test without a proxy: `adb reverse tcp:8080 tcp:8080`, then open `http://localhost:8080` in Chrome on the phone.
The GATT UUIDs are defined in both `src/common/ble_link.h` and `web/index.html`; keep them in sync.

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

- `src/lora_link/`: bidirectional LoRa link test (both boards run it)
- `src/common/heltec_v4.h`: pins and front-end control
- `src/common/ble_link.h`: BLE GATT feed used by the `lora_link_ble` env
- `web/`: phone web app (Web Bluetooth) and its systemd unit, see below
- `src/smoke/`: identity, SX1262 check, GNSS detection
- `docs/`: hardware and radio reference, Heltec's datasheets and schematics
