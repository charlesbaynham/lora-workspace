# Heltec WiFi LoRa 32 V4: the OLED screen

Compiled 2026-11-21 from the schematics and datasheets in
[`heltec-official/`](heltec-official/), Heltec's `Heltec_ESP32` library source,
and the board variant header that PlatformIO already ships. Pin facts also
appear in [`heltec-v4-hardware.md`](heltec-v4-hardware.md).

**Status:** the link firmware (`src/lora_link`, all environments) now drives the screen; see
[What the link firmware shows](#what-the-link-firmware-shows). The panel code has **not been seen running on
a board yet**: the layout was checked by rendering the three pages through U8g2 on a PC, and the build
compiles, but nobody has looked at a real screen. The standalone example further down is older and also unflashed.

## What the link firmware shows

`src/common/oled_ui.h`, called from `src/lora_link/main.cpp`. It shows the numbers the phone app shows, on three
pages that rotate every 5 s; the **PRG** button (GPIO0) steps to the next page. Every page has a title bar with
the link state (*LINKED* / *ONE-WAY* / *LINK DOWN* / *NO PEER*, decided as in the app) and this board's id.

| Page | Contents (app equivalent) |
|---|---|
| signal | peer id and time since its last beacon; RSSI and SNR heard here and heard at the peer (blank unless the link is up and the peer is hearing us); noise floor |
| counts | beacons sent, received, CRC errors and bad %; lost beacons and lost %; frequency error; frequency, SF, bandwidth, power |
| chart | RSSI of the last 104 received beacons, with the noise floor dashed; scale at least -120 to -20 dBm, as in the app |

The screen redraws twice a second; a redraw sends 1 KB over I2C at 400 kHz (about 25 ms), which the radio does not
mind because it holds a received packet until it is read. The rotation also keeps any one page from sitting on the
OLED for days. To change a layout without a board, `draw()` only needs a `U8G2` object and a `Stats` struct, so it
can be run on a PC against a stubbed I2C bus and the buffer dumped as an image.

## Short version

The screen is a 0.96" 128x64 monochrome OLED with an SSD1315 controller, on I2C.
Three things are needed:

1. **Power it:** drive `Vext` (GPIO36) **LOW**. It is off by default.
2. **Talk to it:** I2C address `0x3C`, SDA = GPIO17, SCL = GPIO18. Start `Wire`
   on those pins; the default `Wire` pins (3/4) are wrong.
3. **Reset it:** pulse `RST_OLED` (GPIO21) low then high. The U8g2 and Heltec
   drivers both do this if you hand them the pin.

## Hardware facts

| Item | Value | Source |
|---|---|---|
| Panel | 0.96", 128x64, SSD1315 | both datasheets, "OLED" row of the spec table |
| Interface | I2C, address `0x3C` | `SSD1306Wire(0x3c, ...)` in Heltec's library and V4 factory test |
| SDA / SCL | GPIO17 / GPIO18 | schematics (`OLED_SDA`, `OLED_SCL`), datasheet pin tables |
| RST | GPIO21 | schematics (`OLED_RST`), datasheet pin table |
| Power | `Vext` rail, switched by GPIO36 (`Vext_Ctrl`) | schematics, see polarity below |
| I2C speed | Heltec uses 500 kHz | `SimpleDemo.ino`, factory test |
| Arduino names | `SDA_OLED`, `SCL_OLED`, `RST_OLED`, `Vext` | `pins_arduino.h` in the PlatformIO framework |

The SSD1315 is command-compatible with the SSD1306. Heltec's own driver (`HT_SSD1306Wire`) has the same API as the ThingPulse
`SSD1306Wire` class and sends SSD1306 init commands
(charge pump on, contrast `0xCF`, segment remap + COM scan reversed for the
normal orientation). U8g2 has a dedicated `ssd1315` setup, which is what the
example uses.

### Schematic details (V4.3)

On the V4.3 schematic the screen hangs off a 20-pin 0.4 mm FPC connector (`P2`,
`HC-PBB40C-20DP-0.4V-02`, labelled "OLED Display"). Pin 1 is GPIO18 (SCL),
pin 3 is GPIO17 (SDA), pin 19 is GPIO21 (RST), pins 2/4/6 are `Vext`, and
pins 9/11/14/16/18/20 are GND. The same connector also routes GPIO16, 15, 33,
47, 48 and 43/44/45 to pins that the I2C OLED does not use; presumably they are
for other panels. GPIO17 and GPIO18 also appear on a separate 2-pin header (`P5`).

No I2C pull-up resistors are visible on the main board next to the `OLED_SDA` /
`OLED_SCL` nets, so the pull-ups are presumably on the OLED module. Heltec's
driver never enables the ESP32's internal pull-ups and works, which fits. This
is inference, not something read from a document.

### Vext polarity

`Vext_Ctrl` gates a P-FET (Q2) that has a pull-up on its gate, so the rail is
**off by default and GPIO36 LOW turns it on**. Heltec's library agrees:

```c
#define VEXT_ON_LEVEL  LOW      // heltec.h
#define VEXT_OFF_LEVEL HIGH
```

and so does the V4 factory test (`VextON()` writes `LOW`).

The V4.3.1 datasheet says the pin "needs to be pulled high" (§3.3), which
contradicts its own schematic and the library. The V4.2.0 datasheet says low.
Treat the 4.3.1 sentence as a typo. If the screen stays dark, this is the first
thing to double-check.

`Vext` also feeds the `Ve` pins on the headers, so anything wired there is
powered or unpowered together with the screen.

### Pin conflicts

None with what this repo uses. The LoRa radio uses GPIO8-14, the GNSS connector
uses 34 and 38-42, and the FEM uses 2, 5, 7. GPIO17, 18, 21 and 36 are free for
the screen. `Vext` sits next to `ADC_Ctrl` (GPIO37) and the LED (GPIO35), which
are separate lines.

## Gotchas

- **Wrong default I2C pins.** The V4 `pins_arduino.h` defines `SDA = 3`,
  `SCL = 4` for the general-purpose bus. A bare `Wire.begin()` therefore talks
  to nothing. Use `SDA_OLED` / `SCL_OLED`.
- **Power before init.** Calling `begin()` before `Vext` is low gives a blank
  screen and no error. Heltec sets Vext first, then (in some examples) waits
  100 ms; the example here waits 50 ms.
- **Orientation.** If the picture is upside-down, use `U8G2_R2` instead of
  `U8G2_R0`. Heltec's driver does rotation by changing the SEGREMAP and COM-scan commands
  (`sendScreenRotateCommand()`).
- **Deep sleep.** Heltec's factory test sets Vext high (off) before sleeping. Do
  the same, or the screen keeps drawing current. After wake, set Vext low and
  re-init the display.
- **Burn-in.** It is an OLED. Do not leave a static image on for days.
- **Heltec's `heltec.h`.** The library's `Heltec.begin()` wrapper switches Vext
  on, resets the screen and draws "OLED initial done!". It also includes
  Heltec's own `lora/LoRa.h` class (not read), so it is no use for this
  project's RadioLib firmware. Use the display class on its own, or U8g2.

## Library options

| Library | Verdict |
|---|---|
| **U8g2** (`olikraus/U8g2`) | Recommended. Maintained, has an SSD1315 setup, many fonts, on the PlatformIO registry. Full-buffer mode costs 1 KB RAM, which is nothing on an ESP32-S3. |
| Heltec `HT_SSD1306Wire` / `HT_Display` | Works (it is what Heltec ships), but is bundled in the `Heltec_ESP32` repo, which also pulls in LoRaWAN code and a LoRa library we do not want. |
| Adafruit SSD1306 / ThingPulse `esp8266-oled-ssd1306` | Should work, since the SSD1315 is SSD1306-compatible. Not tried. |

## Working example

Compiled with PlatformIO (`espressif32`, `heltec_wifi_lora_32_V4`,
`olikraus/U8g2@^2.36`) on 2026-11-21. Not run on hardware.

```cpp
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// SDA_OLED / SCL_OLED / RST_OLED / Vext come from the board's pins_arduino.h.
// SSD1315 is driven with U8g2's ssd1315 setup over hardware I2C (addr 0x3C).
U8G2_SSD1315_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, RST_OLED, SCL_OLED, SDA_OLED);

void setup() {
  Serial.begin(115200);
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);  // LOW = Vext on, powers the OLED
  delay(50);                // let the rail settle
  u8g2.begin();             // pulses RST_OLED, starts Wire on 17/18
  u8g2.setFont(u8g2_font_6x10_tf);
}

void loop() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Heltec V4");
  u8g2.setCursor(0, 24);
  u8g2.print(millis() / 1000);
  u8g2.sendBuffer();
  delay(500);
}
```

`platformio.ini` needs `lib_deps = olikraus/U8g2@^2.36` (add it alongside
RadioLib in this repo's `[env]` section, or per environment).

If the screen stays blank, check in this order:

1. Is GPIO36 actually low? (Measure the `Ve` pin: ~3.3 V when on.)
2. Does an I2C scan on SDA=17, SCL=18 find `0x3C`? (If not, power or wiring.)
3. Is the V4 "hardware revision" the one you think it is? The silkscreen reads
   `HTIT-WB32LAF V4.x`. The OLED wiring is the same on V4.2 and V4.3, so this
   should not matter for the screen, only for the LoRa PA.

## Sources

Local:

- `heltec-official/HTIT-WB32LAF_V4.3_schematic.pdf` and
  `WiFi_LoRa_32_V4.2_schematic.pdf` (OLED connector, `OLED_SDA/SCL/RST`, Vext)
- `heltec-official/WiFi_LoRa_32_V4.2.0.pdf` and
  `WiFi_LoRa_32_V4.3.1_Datasheet.pdf` (pin tables, spec table, §3.3 Power Output)
- `.platformio/packages/framework-arduinoespressif32/variants/heltec_wifi_lora_32_V4/pins_arduino.h`
  (`SDA_OLED`, `SCL_OLED`, `RST_OLED`, `Vext`)

Online (read 2026-11-21, `master` branch, not pinned to a commit):

- Heltec library: <https://github.com/HelTecAutomation/Heltec_ESP32>
  - `src/heltec.h` (Vext polarity), `src/heltec.cpp` (init order)
  - `src/HT_SSD1306Wire.h`, `src/HT_Display.cpp` (I2C init commands, reset pulse)
  - `examples/Factory_Test/WiFi_LoRa_32_V4_FactoryTest/` (V4 screen use, `0x3c`, `VextON()`)
  - `examples/OLED/SimpleDemo/`
- U8g2 `ssd1315` setup functions: <https://github.com/olikraus/u8g2> (`csrc/u8g2.h`)
- Heltec wiki, V4 page (nothing on the screen beyond the revision table):
  <https://wiki.heltec.org/docs/devices/open-source-hardware/esp32-series/lora-32/wifi-lora-32-v4/>
