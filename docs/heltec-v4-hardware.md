# Heltec WiFi LoRa 32 V4: hardware reference

Compiled 2026-11-21 from Heltec's own downloads (saved in
[`heltec-official/`](heltec-official/)). Where documents disagree, the
**schematics are treated as the source of truth** and the disagreement is
listed under [Conflicts](#conflicts-between-heltec-documents).

Nothing in the "verified" sections has been tested on hardware yet. It was read
from the schematic images by eye.

## Revisions

| Revision | LoRa front-end (FEM) | Notes |
|---|---|---|
| V4.2 | GC1109 | first official release (datasheet 2025-09-11) |
| V4.3 / V4.3.1 | KCT8103L | wiki: "FEM chip upgraded to KCT8103L", software can bypass the LNA; needs new firmware |

Heltec's Arduino tools menu selects the PA with `USE_GC1109_PA` (V4.2) or
`USE_KCT8103L_PA` (V4.3) ([wiki](https://wiki.heltec.org/docs/devices/open-source-hardware/esp32-series/lora-32/wifi-lora-32-v4/)).
**Which revision our two boards are is not yet known.** The silkscreen on the
back near the GNSS connector should read `HTIT-WB32LAF V4.x`.

Chip: ESP32-S3R2 (2 MB quad PSRAM, so GPIO33-37 are free), 16 MB flash, SX1262.
The V4-R8 is a different board (ESP32-S3R8, different pinout); do not mix up.

## Pins common to V4.2 and V4.3 (schematic and pin map agree)

| Function | GPIO | Notes |
|---|---|---|
| LoRa NSS / SCK / MOSI / MISO | 8 / 9 / 10 / 11 | |
| LoRa RST / BUSY / DIO1 | 12 / 13 / 14 | DIO1 is the IRQ line (Arduino variant names it `DIO0`) |
| OLED SDA / SCL / RST | 17 / 18 / 21 | SSD1315, 128x64 |
| LED (white) | 35 | `LED_Write` |
| Vext_Ctrl | 36 | switches the `Ve` header pins; see conflicts, schematic says **LOW = on** |
| ADC_Ctrl | 37 | drive high to read battery on GPIO1 (`VBAT_Read`); `VBAT = 100/(100+390) * V_adc` |
| USER (PRG) button | 0 | |
| USB D- / D+ | 19 / 20 | native USB (VID 303a, PID 1001) |
| UART0 TX / RX | 43 / 44 | free, since console is on USB-CDC |
| FEM_EN (PA_CSD) | 2 | |
| VFEM_Ctrl | 7 | enable of the LDO that powers the FEM (pulled up by 5.1 MΩ) |

## GNSS connector (SH1.25-8, same in both revisions)

Signals from the schematic (`P3 Header 8`, V4.2 and V4.3). Names are from the
GNSS module's point of view, so `TX_GPS` is an ESP32 **input**.

| Signal | ESP32 GPIO | Direction for ESP32 |
|---|---|---|
| `TX_GPS` (module TX) | **39** | RX, use `Serial1.begin(baud, SERIAL_8N1, 39, 38)` |
| `RX_GPS` (module RX) | **38** | TX |
| `WAKE_UP` | 40 | |
| `PPS` | 41 | |
| `RST_GPS` | 42 | via 499 Ω |
| `VGNSS_Ctrl` | **34** | gate of P-FET Q7 with a 10 kΩ pull-up, so **GPIO34 LOW = GNSS power on**, default off |

Connector pins: 8 = GND, 7 = switched 3V3 (Q7 drain, `VGNSS`), 6 = always-on
3V3, 5 = `TX_GPS`, 4 = `RX_GPS`, 3 = `WAKE_UP`, 2 = `PPS`, 1 = `RST_GPS`.
(Pin order read from the schematic drawing; the datasheet photo shows the
connector's pin 1 at the GPIO42 end.)

A module on pin 6 is powered regardless of GPIO34. A module on pin 7 needs
GPIO34 low.

Typical NMEA defaults (module not identified yet): 9600 baud (L76K family) or
115200 (UC6580 family).

## LoRa PA / front-end control

Per schematic, read by eye:

| FEM pin | V4.2 (GC1109) | V4.3 (KCT8103L) |
|---|---|---|
| CSD (enable) | GPIO2 | GPIO2 |
| CPS (PA path) | GPIO46 | SX1262 DIO2 |
| CTX (TX/RX switch) | SX1262 DIO2 | GPIO5 |
| FEM supply LDO enable | GPIO7 | GPIO7 |

This matches the wiki's V4.3.1 update log, which says GPIO5 became the FEM
control pin and GPIO46 became a free GPIO, and Heltec's own library
(`board-config.h`). The **drive sequence** for TX and RX is in
[`lora-bringup.md`](lora-bringup.md). The GC1109 / KCT8103L datasheets are
still unread, so the full truth tables are unconfirmed. The framework's own
`pins_arduino.h` for the V4 contains none of these pins.

Max TX power: 28 ± 1 dBm (850 mW class). The wiki states 30 dBm is not supported.

## Conflicts between Heltec documents

1. **GNSS supply pin.** Both schematics, the V4.2.0 datasheet and `V4_pinmap.png`
   say `VGNSS_Ctrl` = GPIO34 (the 3V3 pin). The 4.3.1 datasheet figure
   (p. 8) shows the GNSS connector with GPIO37 labelled `VEXT_CTRL` and a 5V
   pin instead. The V4.3 schematic matches GPIO34 / 3V3, so the figure looks
   wrong, but that is unconfirmed on hardware.
2. **Vext polarity.** The V4.2.0 datasheet says Vext_Ctrl "needs to be pulled
   low"; the 4.3.1 datasheet says "pulled high". Both schematics show a P-FET
   (Q2) with a pull-up on its gate, so **low = on**, matching 4.2.0.
3. **GPIO46.** The 4.3.1 datasheet's pin *figure* still shows `FEM_PA` at
   GPIO46, but its pin *table* and the wiki's update log say it is a plain GPIO.
   The V4.3 schematic leaves GPIO46 unconnected.
4. The wiki's own PlatformIO guide uses a custom `heltec_V4` variant and a V3
   board ID, and the variant has no GNSS or FEM pins.

## Sources

Saved locally in [`heltec-official/`](heltec-official/) (Heltec copyright;
personal, non-commercial use):

- `WiFi_LoRa_32_V4.2.0.pdf`, `WiFi_LoRa_32_V4.3.1_Datasheet.pdf` (datasheets)
- `WiFi_LoRa_32_V4.2_schematic.pdf`, `HTIT-WB32LAF_V4.3_schematic.pdf`
- `V4_pinmap.png` (pin map, V4.2-era)

Online:

- Wiki: <https://wiki.heltec.org/docs/devices/open-source-hardware/esp32-series/lora-32/wifi-lora-32-v4/>
  (and its `hardware-update-log` and `frequently-asked-questions` subpages;
  these were read by a fetch agent and summarised, not read first-hand)
- Downloads index: <https://resource.heltec.cn/download/WiFi_LoRa_32_V4>
- Heltec Arduino library (not yet read): <https://github.com/HelTecAutomation/Heltec_ESP32>
- Third-party reports of the FEM pins (unverified, partly wrong for V4.3):
  [TTN forum thread](https://www.thethingsnetwork.org/forum/t/heltec-wifi-lora-32-v4-need-help-custom-firmware-tx-reports-success-but-rx-never-receives-heltec-v4-heltec-v4/72874.md),
  [RadioLib discussion #1665](https://github.com/jgromes/RadioLib/discussions/1665)
