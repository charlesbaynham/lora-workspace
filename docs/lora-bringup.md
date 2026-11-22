# LoRa bring-up on the Heltec WiFi LoRa 32 V4

**Status (2026-11-21): working.** Two boards exchange LoRa packets in both
directions with no CRC errors (about 30 packets each way over several runs).
One problem found and fixed: a poorly seated antenna made the link 46 dB
asymmetric ([details](#the-46-db-asymmetry-poorly-seated-antenna)). After
reseating, the two directions agree to 1 dB.

## Run it

Flash the same firmware to both boards, then watch their serial output. Fish
syntax; ports as in `scripts/ports.sh`:

```fish
set -x PLATFORMIO_CORE_DIR $PWD/.platformio
uvx --from platformio pio run -t upload --upload-port /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_90:DA:72:FB:75:FC-if00
uvx --from platformio pio run -t upload --upload-port /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_90:DA:72:FE:0A:AC-if00
uv run --with pyserial python -I scripts/serial_capture.py /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_90:DA:72:FB:75:FC-if00 30
```

Each board beacons `V4 <id> #<n>` every 4-6 s and otherwise listens, printing
RSSI, SNR, frequency error and an idle noise-floor reading.

| Env | For | Difference |
|---|---|---|
| `lora_link` | V4.3 boards (KCT8103L PA), **the firmware, on both** | default |
| `fem_sweep` | diagnostic | RX only: steps GPIO7/2/5 through all 8 states and reports noise floor and RSSI (needs another board running `lora_link`) |
| `smoke` | either | identity, SX1262 check, GNSS detection |

Both boards are V4.3 (confirmed from the silkscreen), so both run `lora_link`.

The build variants named in the measurements below (`lora_link_lna`, `lora_link_nognss[_lna]`, `lora_link_v42`) were
removed when the project moved to a single firmware. They are in git history before that change. The firmware has
the receive LNA off, does not touch the GNSS module, and supports V4.3 boards only.

## What the radio needs

All three of these are required; none is the RadioLib default.

1. **TCXO on DIO3, 1.8 V.** The SX1262 powers a TCXO, not a crystal. Pass
   `tcxoVoltage = 1.8` to `radio.begin()`.
2. **DIO2 as RF switch.** `radio.setDio2AsRfSwitch(true)`. DIO2 drives the front
   end's TX/PA-path pin on its own.
3. **DC-DC regulator** (`useRegulatorLDO = false`).

Pins: NSS 8, SCK 9, MOSI 10, MISO 11, RST 12, BUSY 13, DIO1 14.

On top of that the **front end** must be powered and enabled by GPIO, or the
radio is silent or deaf.

## Front-end control

From Heltec's own library (`HelTecAutomation/Heltec_ESP32` at commit `6ab5cf9`,
2026-08-24: `src/driver/board-config.h`, `src/radio/radio.c` `RadioSend` /
`RadioRx`, `src/driver/sx126x.c` `SX126xSetTxParams`). Pins agree with the
schematics ([hardware reference](heltec-v4-hardware.md)).

### V4.3 and V4.3.1: KCT8103L (Heltec's default)

| Pin | Net | Level |
|---|---|---|
| GPIO7 | FEM supply LDO enable | HIGH, always; wait 1 ms |
| GPIO2 | CSD (chip enable) | HIGH, always; wait 1 ms |
| GPIO5 | CTX | **TX: HIGH.** RX: HIGH = LNA bypassed (Heltec default), **LOW = LNA on** |
| DIO2 | CPS (PA path) | driven by the SX1262 |

Heltec exposes the RX choice as `LORA_RX_LNA` (Arduino menu "LoRa External FEM
Receive Gain": Disabled = 1, Enabled = 0, from `WiFi_Kit_series/boards.txt`).
Measured on `FB75FC`: enabling the LNA raised RSSI and noise floor by about
22 dB. **Use it only for weak signals.** With the boards close together the
signal is about -12 dBm, and 22 dB of LNA gain would put roughly +10 dBm into the
SX1262, which is at or beyond its input limit (I recall about +10 dBm absolute
maximum; check the SX1262 datasheet). Leave it off when boards are within a few
metres of each other.

### V4.2: GC1109

| Pin | Net | Level |
|---|---|---|
| GPIO7 | FEM supply LDO enable | HIGH |
| GPIO2 | CSD (enable) | HIGH; wait 1 ms |
| GPIO46 | CPS (PA path) | HIGH for TX; wait 2 ms |
| DIO2 | CTX | driven by the SX1262 |

Heltec never drives GPIO46 low again in `RadioRx`; the removed `lora_link_v42` build did. Neither
choice is verified. The GC1109's truth table is not in any document I have.

### Deep sleep

Heltec holds the FEM pins across deep sleep with `rtc_gpio_hold_en()` (GPIO2,
and GPIO7 on V4.3) and calls `rtc_gpio_hold_dis()` on wake. Do the same, or the
pins float or stay latched. Not implemented here.

## TX power

The SX1262 drives the PA; the PA adds gain. Heltec's table for the KCT8103L is
13 dB up to about +13 dBm out, falling to 7 dB at +21 (`kct8103l_tx_gain[]` in
`sx126x.c`). The SX1262 setpoint is the wanted output minus the gain, clamped to
-9..+22 dBm.

Consequences:

- The lowest reachable output is about **+4 dBm** (setpoint -9 dBm).
- `lora_link` asks for +10 dBm out, so the setpoint is -3 dBm.
  `heltec_v4.h` hard-codes the 13 dB figure; it is only valid up to about
  +13 dBm out. Above that, port Heltec's table.
- Heltec's V4.2 (GC1109) uses a cubic fit, not a table:
  `setpoint = floor(0.0004 x³ - 0.011 x² + 1.0866 x - 11.365)`, x = wanted output.
- Maximum is 28 ± 1 dBm per the datasheet. **Not tested.** Do not go above
  the limit for your band (see below).

## RadioLib recipe

```cpp
SX1262 radio = new Module(8 /*NSS*/, 14 /*DIO1*/, 12 /*RST*/, 13 /*BUSY*/);
femInit();                                   // GPIO7, GPIO2, GPIO5 high
SPI.begin(9, 11, 10, 8);                     // SCK, MISO, MOSI, NSS
radio.begin(868.1, 125.0, 7, 5, RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
            10 - 13 /*dBm at SX1262*/, 8, 1.8 /*TCXO V*/, false /*DC-DC*/);
radio.setDio2AsRfSwitch(true);
radio.setPacketReceivedAction(onPacket);     // sets a flag from the ISR
radio.startReceive();
// to send: clearPacketReceivedAction(); femTx(); transmit(...); then re-arm RX
```

`radio.transmit()` polls DIO1 itself, so clear the receive interrupt action
around it (as `src/lora_link/main.cpp` does) or the ISR will set a spurious
receive flag.

## Link state and the beacon

Beacon text is `V4 <id> #<seq>`, plus ` <rssi>/<snr>` once the sender has heard the
other board in the last 20 s (that is what the sender measured from us). Each board
therefore knows both directions of the link. Per board, `src/lora_link/main.cpp` keeps:

- **linked**: a beacon from the peer arrived within `LINK_TIMEOUT_MS` (20 s, about 3
  missed beacons). Printed as `LINK UP` / `LINK DOWN` on serial, and every 10 s.
- **lost**: gaps in the peer's sequence numbers. A sequence that goes backwards is
  treated as a peer reboot, not a loss.
- **peer hears us**: the peer's latest beacon carried a report. No report while we hear
  it means the link is one-way (see the antenna note below).

Only one peer is tracked. The same state goes to the
phone as a `link` event every 2 s (and on every change); see `web/index.html`.

## Regulatory (UK/EU)

`lora_link` uses 868.1 MHz, SF7, BW 125 kHz: sub-band g1 (868.0-868.6 MHz),
limited to 1 % duty cycle and 14 dBm ERP. The beacon is 48 ms on air, or 58 ms
once it carries the peer report (see below), sent every 5-7 s (mean 6 s): about
0.97 %, so there is almost no headroom for a longer beacon or a shorter interval.
This is a note, not legal advice; check the current Ofcom / ETSI rules
before changing frequency, power or rate.

## Measurements

Boards about 30 cm apart, +10 dBm out, SF7 / BW125. `FB` = `FB75FC` (had the
GNSS module attached until the "unplugged" row), `FE` = `FE0AAC`. Both boards
V4.3. **Every row above the "reseated" row was taken with a poorly seated
antenna** (most likely on FE), so only the last row is a trustworthy link
budget; the others record the fault.

| Run | FB hears FE | FE hears FB | Noise floor FB / FE |
|---|---|---|---|
| Both `lora_link` (RX LNA off) | -79 dBm, SNR 8.8 | -33 dBm, SNR 12.8 | -91 / -104 dBm |
| FE on `lora_link_v42` (GPIO46 driven) | -79 | -33 | |
| FB on `lora_link_v42`, FE on `lora_link` | -79 | -33 | |
| Both `lora_link_lna` (RX LNA on) | **-57**, floor **-68** | **-33**, floor **-104** (unchanged) | |
| FB GNSS off + in reset, LNA off | -81 | -35 | -91 / -95 |
| FB GNSS off + in reset, LNA on | -59, floor -68 | -35, floor -95 (unchanged) | |
| **GNSS module physically unplugged**, LNA off | **-77**, SNR 11.5 | **-32**, SNR 12.5 | **-98** / -101 |
| FB also moved off the laptop, similar places | **-74**, SNR 12.5 | **-27**, SNR 12.8 | **-104** / -97 |
| **Both antennas unscrewed and reseated** | **-12**, SNR 12.0 | **-11**, SNR 12.5 | -88 / -90 |

The noise-floor reading is the median of 15 instantaneous RSSI samples in RX
(`getRSSI(false)`), so it includes whatever is in the band. FE's floor drifted
from -104 to -95 dBm between sessions with no firmware change, so differences of
about 10 dB are not significant. Frequency error between the two TCXOs is about
±200-300 Hz, drifting as the boards warm up.

## The 46 dB asymmetry: poorly seated antenna

**Symptom.** FB heard FE about 47 dB weaker than FE heard FB (-74 vs -27 dBm),
with identical firmware.

**Cause.** An antenna that was not properly seated on its connector. After both
antennas were unscrewed and screwed back in, the link became symmetric: -12 dBm
one way, -11 dBm the other, with a -88/-90 dBm noise floor. Which antenna was
loose is not certain; the evidence below points at FE's.

**Lessons.**

- My first analysis said antennas could not cause an asymmetry, because antenna
  gain appears in both directions and cancels. That is true for a good antenna
  path and was wrong here. A badly connected antenna lets the transmitter leak
  and the receiver pick up signal by other routes (stray coupling around the
  front end), and those routes are not reciprocal. **Check antenna seating
  first** when a link looks lopsided.
- It explains the odd front-end sweep result. While FE's antenna was loose, FE's
  receiver ignored all three front-end pins, because the signal it heard was
  reaching the radio by stray coupling rather than through the front end. After
  reseating, FE responds like FB (below). My intermediate hypothesis that FE was
  a no-front-end "-N" variant was wrong. Both boards are the same model.
- Noise readings were also contaminated: part of the apparent noise-floor
  difference between boards, and some of the "GNSS module adds noise" and
  "laptop adds noise" findings, may have come from the poor connection. Treat
  those as unproven.

### Front-end sweep with antennas reseated (`fem_sweep`, RX only)

| GPIO7 / GPIO2 / GPIO5 | FE noise floor | FE RSSI of FB |
|---|---|---|
| 1 / 1 / 1 (Heltec default, LNA bypassed) | -90 | -11 |
| 1 / 0 / 1 and 1 / 0 / 0 (CSD low, FEM shut down) | -113 | **-50** |
| 0 / x / x (FEM unpowered) | -101 | -21 |

The LNA-on state was omitted (see warning above). Earlier, with FB's antenna
fine and the boards 30 cm apart, FB showed the same pattern, plus +23 dB with
GPIO5 low. So the front-end control pins and sequence in this document are
correct for both boards.

### Still open

- **Range and sensitivity** are untested; at 30 cm the signal is far above the
  noise. Measure at distance, and with SF/BW changes, before claiming a figure.
- **Only 5-10 minutes of captures** per configuration. A longer soak (hours,
  with counters) has not been done.
- **TX power** above +10 dBm and the PA gain table above +13 dBm out are untested.

## Sources

- Heltec library: <https://github.com/HelTecAutomation/Heltec_ESP32> (read at `6ab5cf9`)
- Heltec Arduino core board menu: <https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series> (`boards.txt`)
- Schematics and datasheets: [`heltec-official/`](heltec-official/)
- Wiki: <https://wiki.heltec.org/docs/devices/open-source-hardware/esp32-series/lora-32/wifi-lora-32-v4/>
