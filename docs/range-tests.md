# Link measurements and antenna changes

A log of measured link strength between the two boards, with the conditions, so numbers
taken at different times can be compared fairly. Radio settings are in
[`lora-bringup.md`](lora-bringup.md): 868.1 MHz, SF7, BW 125 kHz, +10 dBm out. Firmware
`lora_link_ble` (same link code as `lora_link`). `FB` = `FB75FC`, `FE` = `FE0AAC`.

Read RSSI, not SNR: SNR sits at about 12 dB in every measurement here, including the 30 cm
one at -12 dBm, so it does not tell the cases apart.

## Placement

From 2026-11-22 the boards are in **different rooms of the same flat: one in the living room,
one in the bedroom**: roughly **20 m apart with about 2.5 walls between them** (owner's
estimate, "ish"). Which board is in which room is not recorded; add it when known.

## Measurements

| Date | Antennas | Placement | RSSI FB→FE | RSSI FE→FB | Noise floor | Notes |
|---|---|---|---|---|---|---|
| earlier | stock | 30 cm apart, same bench | -11 dBm | -12 dBm | -88 / -90 dBm | after reseating both antennas; from `lora-bringup.md`. Near-field, not comparable with the rows below |
| 2026-11-22, before swap | stock | not recorded | -73 to -74 dBm | not recorded | not recorded | read off FB's log ("peer hears us at") right after flashing it |
| 2026-11-22, after swap | larger antennas (model not recorded) | living room / bedroom, ~20 m, ~2.5 walls | -57 to -58 dBm | -58 dBm | -89 dBm | 60 s capture on FE, 10 of 10 packets received, none lost |

## Reading it

- After the antenna swap the link was about 16 dB stronger than the earlier same-day
  reading, and symmetric (-58 dBm both ways), so neither antenna looks badly seated. The noise
  floor did not change, so the gain is in the signal.
- That 16 dB is **not** a clean antenna result. The earlier reading has no recorded placement,
  so any move of the boards while swapping would be mixed in. Each antenna adds its gain at
  both ends, so a large antenna gain is plausible, but this log cannot separate it from distance.
- For a controlled comparison: fix the placement, note it here, and measure with each
  antenna set in the same positions and orientation (the 30 cm bench setup is the easiest to
  repeat).

## Sanity check against free space

Free-space path loss at 868.1 MHz is 20.7 dB at 0.3 m and 57.2 dB at 20 m. The firmware asks
for +10 dBm at the antenna port, so with antenna gains and wall losses added:

    RSSI = 10 dBm - FSPL + (Gtx + Grx) - wall losses

- **30 cm bench, -11 to -12 dBm:** the formula gives -10.7 dBm for 0 dBi antennas, so the old
  antennas behaved like ~0 dBi, and the model is about right (at 30 cm it is only a rough one).
- **20 m, new antennas, -58 dBm:** the formula gives -47 dBm for 0 dBi antennas and no walls, so
  walls minus extra antenna gain come to about 11 dB. That fits 2.5 walls at roughly 4 dB each
  with unremarkable antennas, or larger walls losses (~7 dB each) with several dB of extra gain.
  Both are plausible; this reading alone cannot tell them apart.
- **Earlier reading, -73 to -74 dBm:** about 15 dB below that model for the same walls and 0 dBi
  antennas. At the same 20 m placement the old antennas would have to have been very poor; it is
  more likely that the placement then was different (further, or more walls), or an antenna was
  not seated well. Treat the 16 dB improvement as unexplained until it is measured in one fixed
  placement.

The wall loss per wall is a typical-value guess (a few dB for interior walls at 868 MHz), not
measured here.

## How to measure

Both boards run `lora_link`/`lora_link_ble`. Capture 60 s on one board and read the `RX` lines
(what it hears) and `LINK ... peer hears us at` (what the other board hears of it):

```fish
uv run --with pyserial python -I scripts/serial_capture.py <port> 60 | grep -E '^(RX|LINK|noise)'
```

Or connect the phone web app to each board: it shows both directions and the noise floor.
