# docs

Reference material for the Heltec WiFi LoRa 32 V4 boards.

- [`lora-bringup.md`](lora-bringup.md): getting the LoRa radio working: SX1262
  settings, front-end (PA) control for V4.2 and V4.3, TX power, measurements, and
  open issues. Start here for radio work.
- [`heltec-v4-hardware.md`](heltec-v4-hardware.md): pin map, GNSS wiring, PA/FEM
  control, and known conflicts between Heltec's documents. Start here.
- [`battery.md`](battery.md): LiPo charging (CN3165, 540 mA fixed), runtime
  estimates, and solar panel sizing with firmware power targets.
- [`oled-display.md`](oled-display.md): how to power and drive the on-board
  SSD1315 OLED (Vext, I2C pins, library choice, compiling example).
- [`range-tests.md`](range-tests.md): log of measured link strength (RSSI) between the two
  boards with antennas and placement, so readings can be compared fairly.
- [`ble-ota.md`](ble-ota.md): updating the firmware from the phone over BLE:
  protocol, pairing passkey, rollback, limits.
- [`wifi-ota.md`](wifi-ota.md): choosing a Wi-Fi network from the web app (stored on the
  board, not in the repository) and updating the firmware over Wi-Fi instead of BLE.
- [`heltec-official/`](heltec-official/): Heltec's datasheets, schematics and
  pin map, downloaded unmodified from `resource.heltec.cn`. Copyright Heltec
  Automation; kept here for personal, non-commercial reference.

When something about the hardware is in doubt, check the schematic PDFs before
guessing or probing pins empirically.
