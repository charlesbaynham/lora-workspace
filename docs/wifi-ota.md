# Wi-Fi setup and firmware updates over Wi-Fi

A faster alternative to the BLE update ([`ble-ota.md`](ble-ota.md), which stays as the field
option): the phone web app tells the board which Wi-Fi network to use, and then tells it to
download the published firmware itself. Code: `src/common/ble_wifi.h` (board) and the "Wi-Fi" card
and *Update over Wi-Fi* button in `web/index.html` (phone).

**No Wi-Fi name or password is in the repository or in the firmware image.** They are typed into the
web app, sent over the paired BLE link, and kept in the board's own flash (NVS, namespace `wifi`).

## Using it

1. Publish and deploy the firmware as usual (`scripts/publish_firmware.sh`, commit, push). The
   first image with Wi-Fi support has to reach each board by BLE or USB; after that it is optional.
2. Open the deployed page (not `localhost`; the board has to be able to reach it), connect to a
   board. If it runs a Wi-Fi capable build, a "Wi-Fi" card appears.
3. *Choose network…*: the board scans (a few seconds, LoRa beacons are missed meanwhile), the page
   lists what it found. Tap one, enter the password, *Connect and save*. The board joins, and only if
   that worked saves the network and tells you its IP address. A wrong password is not saved.
4. *Update over Wi-Fi* (in the Firmware update card): the board joins the saved network, downloads
   `firmware/lora_link_ble.bin` from the page's own address, checks size and SHA-256, installs it and
   reboots. The phone only has to stay connected for the start and the progress display.
5. Do step 3 once per board. *Forget* removes the saved network.

The saved network survives firmware updates (BLE or Wi-Fi) and USB uploads (`pio run -t upload`
rewrites the app and bootloader, not NVS). It is lost only by `pio run -t erase` or a full-flash erase.

## How it works

One characteristic on the existing service (UUID `6e4c0006-…`, in `ble_wifi.h` and `index.html`),
read/write/notify, all authenticated like the update ones. Commands and notifications are listed at
the top of `ble_wifi.h`. The BLE callbacks only queue the job; `ble_wifi::loop()` runs it, because a
scan or download takes seconds and must not stall the BLE host task.

- **Wi-Fi is off** except while a scan, connect or update is running, so normal operation and the
  868 MHz noise floor are as with the BLE-only build.
- **Where the image comes from:** the phone gives the board the URL (`firmware/lora_link_ble.bin`
  resolved against the page address) and the SHA-256 from `firmware/manifest.json`
  (`scripts/publish_firmware.sh` writes it). The board hashes what it downloads and only boots it if
  the hash matches. If the deployed manifest and image disagree, for example mid-deploy, the update
  fails with "sha256 mismatch" and the old firmware keeps running.
- **TLS:** HTTPS works, but the certificate is *not* validated (`setInsecure()`), because the SHA-256
  checked over the paired BLE link is what authenticates the image, and it spares the flash for a CA
  bundle. Plain `http://` also works.
- **Rollback** is the same as for BLE updates (new image must run 15 s before it is kept).

## Security

- The saved password is in NVS in clear text (no flash encryption on these boards); anyone with USB
  access to the board can read it. It is never sent back over BLE: the read returns the network
  name only, and only after pairing.
- As with the BLE update, the pairing passkey is fixed in firmware (`OTA_PASSKEY`, default `123456`,
  which is public in this repository). Anyone in BLE range who knows it can pair, scan, change the
  saved network, or make the board fetch a firmware image of their choosing from a URL and hash they
  supply. Change the passkey for anything beyond a bench.

## Limits

- 2.4 GHz networks only. Open and WPA/WPA2/WPA3 personal work; networks that need a login page
  (captive portals) or enterprise (802.1X) logins do not.
- Network names are handled as UTF-8; one with invalid UTF-8 bytes cannot be selected from the list.
- The image is 1,246,464 bytes. The old 4 MB partition table's 1.25 MiB slots would have left 5 % spare,
  so the project now uses `default_16MB.csv` (6.25 MB slots). **Each board needs one USB upload to get
  the new table**; it cannot be changed over BLE or Wi-Fi (see [`ble-ota.md`](ble-ota.md#limits)).
  Until then an updated board still works, but is limited to 1.25 MiB images.
- Each published image is now about 1.2 MB in git history.
- Untested on hardware at the time of writing: the page logic was run against a mock board only.
  Wi-Fi scanning and BLE share the 2.4 GHz radio, so a BLE disconnect during a scan or update is
  possible; the web app reconnects by itself.
