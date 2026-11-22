# Link measurements and antenna changes

A log of measured link strength between the two boards, with the conditions, so numbers
taken at different times can be compared fairly. Radio settings are in
[`lora-bringup.md`](lora-bringup.md): 868.1 MHz, SF7, BW 125 kHz, +10 dBm out. Firmware
`lora_link`. `FB` = `FB75FC`, `FE` = `FE0AAC`.

Read RSSI, not SNR: SNR sits at about 12 dB in every measurement here, including the 30 cm
one at -12 dBm, so it does not tell the cases apart.

## Placement

From 2026-11-22 the boards are in **different rooms of the same flat: one in the living room,
one in the bedroom**: roughly **20 m apart with about 2.5 walls between them** (owner's
estimate, "ish"). Which board is in which room is not recorded; add it when known.

**Antenna orientation (owner's account).** Before the swap one antenna was lying so that it
pointed straight at the other room, "pretty much the worst possible position". Now it stands
upright, so the dipole is broadside to the other room. A dipole radiates and receives
best perpendicular to its axis and has a null along it, so this is the best case and the old
one was the worst. The orientation of the antenna at the other board, and which board is
which, are not recorded.

## Measurements

| Date | Antennas | Placement | RSSI FB→FE | RSSI FE→FB | Noise floor | Notes |
|---|---|---|---|---|---|---|
| earlier | stock | 30 cm apart, same bench | -11 dBm | -12 dBm | -88 / -90 dBm | after reseating both antennas; from `lora-bringup.md`. Near-field, not comparable with the rows below |
| 2026-11-22, before swap | stock | same two rooms as the row below (owner confirmed) | -73 to -74 dBm | not recorded | not recorded | read off FB's log ("peer hears us at") right after flashing it; one direction only, a handful of packets. Antennas were seated properly (the early mis-seated antenna was an earlier session) |
| 2026-11-22, after swap | larger antennas (model not recorded) | same rooms, ~20 m, ~2.5 walls; **antenna orientation also changed**, see below | -57 to -58 dBm | -58 dBm | -89 dBm | 60 s capture on FE, 10 of 10 packets received, none lost |

## Reading it

- Between the "before" and "after" readings **two things changed**: the antennas (stock to
  larger) and the orientation of at least one of them, from end-on towards the other room (the
  null of a dipole) to broadside (its maximum). The received signal went up by about **16 dB**
  (-73 to -74 dBm to -57 to -58 dBm), with the noise floor unchanged at about -89 dBm, and the
  link is now symmetric (-58 dBm both ways), so neither antenna looks badly seated.
- **The orientation change alone can plausibly account for most of the 16 dB.** An ideal dipole
  has a deep null end-on; indoors, reflections fill it in, but a drop of 10 to 20 dB between
  end-on and broadside is typical. So the larger antennas probably contributed only a few dB,
  and the swap's measured gain should not be credited to antenna size. This is an inference
  from the geometry the owner described, not a measurement.
- Limits of the number: the "before" reading is one direction (FE hearing FB) from a handful
  of packets, and indoor fading adds several dB of its own.
- To separate the two: keep the position fixed, change only the antenna (or only the
  orientation of one antenna, end-on versus broadside), and capture a few minutes in both
  directions each time. Note the positions in this file.
- The much earlier readings in `lora-bringup.md` (-74 vs -27 dBm, the 46 dB asymmetry) were
  taken while one antenna was not seated properly, and are not comparable with any of these.

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
- **Old antennas, same placement, -73 to -74 dBm:** about 26 dB below the no-wall 0 dBi figure.
  If the old antennas really were ~0 dBi (the bench result), the walls cost about 25 dB, about
  10 dB each, which is heavy but possible for brick or concrete. Then the new -58 dBm needs
  about +14 dB of combined antenna gain (about +7 dBi each), which is high for ordinary whips
  (a few dBi). The two anchors do not reconcile cleanly: the 30 cm bench is near-field and a
  poor reference, and indoor fading adds several dB either way.
- **Conclusion:** the setup works much better now, and the most likely reason is the
  antenna orientation (end-on, in the null, to broadside), with the larger antennas a smaller
  part. That matches the budget: 16 dB is high for antenna gain alone but ordinary for an
  orientation change. The budget has two guessed numbers (wall loss, old antenna gain) and
  cannot separate the two.

The wall loss is a guess (a few dB per interior wall is typical; the old-antenna figure above implies
more), not measured here.

## How to measure

Both boards run `lora_link`. Capture 60 s on one board and read the `RX` lines
(what it hears) and `LINK ... peer hears us at` (what the other board hears of it):

```fish
uv run --with pyserial python -I scripts/serial_capture.py <port> 60 | grep -E '^(RX|LINK|noise)'
```

Or connect the phone web app to each board: it shows both directions and the noise floor.
