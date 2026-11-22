# Firmware updates over BLE

The firmware can be updated from the phone web app (`web/index.html`),
with no USB cable. Code: `src/common/ble_ota.h` (board) and the "Firmware update" card in
`web/index.html` (phone).

## Using it

The web app installs the image stored in the repository, `web/firmware/lora_link.bin`; it
never builds anything. To publish a new version: `scripts/publish_firmware.sh` (builds
`lora_link`, copies `firmware.bin` there and writes `web/firmware/manifest.json` with the
build stamp and size), then commit both files. Pushing to `master` deploys them with the page.

1. Open the web app, connect to a board. The "Firmware update" card appears if that board's
   firmware supports it, and says which build it runs and which is published.
2. Tap *Update firmware*. The first time, Android asks to pair: enter the
   passkey (123456 unless you set `-DOTA_PASSKEY=<n>` in `build_flags`; do change it).
3. It takes about 3 minutes for 600 KB (about 4 KB/s, one acknowledged write per connection
   interval). Keep the screen on. The board restarts and the app reconnects and reports the new
   build time (`__DATE__ __TIME__` of `main.cpp`'s last compile).

For a much faster update at home, see [`wifi-ota.md`](wifi-ota.md).

Update each board separately. The very first OTA-capable build has to go over USB.

## How it works

Characteristics on the existing service (UUIDs in both `ble_ota.h` and `index.html`):

| UUID suffix | Name | Properties | Use |
|---|---|---|---|
| `6e4c0004-…` | CTRL | write (authenticated), notify | commands in, JSON status out |
| `6e4c0005-…` | DATA | write (authenticated) | raw image bytes, 240 per write, with response |

CTRL commands: `01 <u32 LE size> <32-byte SHA-256>` begin, `02` end, `03` abort. Status:
`{"ota":"ready"}`, `{"ota":"done"}`, `{"ota":"err","m":"..."}`.

- The image is written to the *other* OTA slot (`app0`/`app1`, 6.25 MB each in the 16 MB table
  `default_16MB.csv`, set in `platformio.ini`). The running firmware is
  untouched until `end` has checked the size, the SHA-256 and the ESP image header, so an
  interrupted or corrupt transfer just leaves the old firmware running.
- Writes use write-with-response. Write-without-response is faster but can be dropped while the
  BLE host task is blocked on flash.
- **Security:** CTRL and DATA need an authenticated pairing (passkey), so a stranger's phone
  cannot flash the board. The passkey is fixed in firmware (the boards have no input); anyone who
  knows it and is in range can update. The status feed itself stays open.
- **Rollback:** the bootloader has app rollback enabled. `lora_link` overrides
  `verifyRollbackLater()` so a freshly updated image is only kept once it has run 15 s
  (`ble_ota::confirmBootWhenHealthy()`); if it resets before that, the bootloader boots the
  previous image. The web app tells you if the build did not change after an update.
- A USB upload (`pio run -t upload`) rewrites `otadata`, so it always boots the freshly
  uploaded image, whichever slot was active.

## Limits

- Installing a diagnostic image (`smoke`, `fem_sweep`) removes OTA from the board; recover over USB.
- **The partition table is only written by a USB upload, never over the air.** A board flashed
  before the switch to `default_16MB.csv` still has the old 4 MB table with 1.25 MB slots, whatever
  image it is updated with; give it one USB upload (`pio run -t upload`) to switch.
  The new table keeps NVS at the same address, so saved settings (the Wi-Fi network) survive.
  An image larger than the board's slot is refused by the update ("Update.begin" fails).
- Each published image adds ~600 KB to the git history.
- Speed: the phone, not the board, picks the connection interval; the board asks for 15-30 ms.
