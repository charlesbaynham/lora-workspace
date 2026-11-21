# Battery and solar power (Heltec V4.x)

Compiled 2026-11-21 from `heltec-official/HTIT-WB32LAF_V4.3_schematic.pdf` (read by eye)
and the V4.3.1 datasheet. **Nothing here has been measured on hardware.** Sunshine
figures are typical UK values from memory, not looked up (PVGIS would give proper ones).

## Summary

- Charging is automatic: plug a cell into the battery connector and power the board over
  USB-C or the solar connector.
- Charge current is fixed at about 540 mA, which is 1.8C for a 300 mAh cell. Check the
  cell's rating.
- The current `lora_link` firmware draws about 77 mA, so a 300 mAh cell lasts about
  3-3.5 hours.
- A 75 mA, 5 V solar panel cannot sustain that. It supports an average of about 8-10 mA
  in sunny summer, about 4 mA in spring or autumn and about 1.2 mA in December. Running
  from solar means rewriting the firmware to sleep (target 3 mA or less).

## Hardware

| Item | Detail |
|---|---|
| Charger | **CN3165** (Consonance), linear Li-ion/LiPo, on the board |
| Battery connector | SH1.25-2P, `JP2`, net `VBAT` |
| Solar connector | SH1.25-2P, `JP3`; same size as the battery one, do not mix them up |
| Solar input range | 4.7-6 V (datasheet table 3.2). USB and the 5V pin are exclusive; solar and battery can be connected with either |
| Battery range | 3.3-4.2 V, 250 mA or more (datasheet table 3.2) |
| Charge LED | `LED2` on the `CHRG` pin (schematic; not seen on hardware) |
| Battery protection | None visible on the board's battery path. The cell needs its own protection circuit (PCM) |

Polarity: the schematic suggests pin 1 of `JP2` is VBAT, but pin 2 was cut off in the
crop I read. Check with a meter before the first plug-in, since many cells ship wired the
opposite way round for Heltec boards.

## Charging

- **Charge current is about 540 mA.** The schematic annotates `I = 1188 / R13 = 540 mA`
  with R13 = 2.2 kΩ on ISET. For a 300 mAh cell that is 1.8C; most small LiPos are rated
  0.5-1C.
- To change it, replace R13 (SMD rework): about 3.9 kΩ gives 300 mA (1C), about 8.2 kΩ
  gives 150 mA (0.5C).
- Datasheet table 3.4 lists charging draw as 500 mA at 5 V.
- Cold: LiPo must not be charged below 0 °C. The `TEMP` pin appears to be tied to ground
  in the schematic, which would mean no temperature cutoff. Confirm before leaving a
  board outdoors in winter.
- Not confirmed: CN3165 termination threshold, trickle-charge behaviour, timers, maximum
  input voltage. The datasheet PDF could not be fetched (radiolocman and
  consonance-elec.com returned HTML wrappers). A listing says only: 4.2 V regulation,
  thermal foldback, auto-recharge, under 3 µA drain with no input, and that it adjusts
  charge current to the input supply's capability (suits solar).

## Runtime on battery (estimates)

Datasheet (table 3.4): RX 75 mA, TX 14 dBm 290 mA, battery "sleep" 20 µA (deep sleep,
cannot listen).

| Scenario | Average current | 300 mAh cell (about 240-270 mAh usable) |
|---|---|---|
| `lora_link` as written (continuous RX, 1% TX beacons) | about 77 mA | **about 3-3.5 h** |
| Same, with a GNSS module powered (assumed +25 mA, unverified) | about 100 mA | about 2.5 h |
| Radio duty-cycled + CPU light sleep (needs firmware work) | about 5-10 mA | about 1-2 days |
| Deep sleep | about 20 µA | months (self-discharge dominates), but not listening |

## Solar

### Sizing for the current firmware

At 77 mA the load is about 1.85 Ah per day on the battery side, about 2.05 Ah per day
from the panel. Panel size needed, with 30% off for tilt, dirt and angle:

| Firmware | Average draw | Summer (about 4 sun-hours) | December (about 0.6 sun-hours) |
|---|---|---|---|
| As is (continuous RX) | 77 mA | about 4-5 W (about 0.75 A) | about 30 W (about 5 A) |
| Duty-cycled radio + light sleep | about 8 mA | about 0.5 W | about 3 W (about 0.5 A) |

The winter night alone (about 16 h) needs about 1.2 Ah at 77 mA, far more than a
300 mAh cell holds. At 8 mA it needs about 130 mAh, which the cell covers.

### The 75 mA, 5 V panel

The 75 mA rating is full sun, facing the sun. Assume 70% of that after real-world losses
and 90% charge efficiency, giving about 47 mAh delivered per full-sun hour. The
sustainable average draw is that daily total divided by 24 h:

| Conditions | Full-sun hours/day | Delivered | Sustainable average draw |
|---|---|---|---|
| Sunny summer | 4-5 | 190-235 mAh | **8-10 mA** |
| Spring or autumn | about 2 | about 95 mAh | **about 4 mA** |
| December | about 0.6 | about 28 mAh | **about 1.2 mA** |
| Dim December | about 0.3 | about 14 mAh | about 0.6 mA |

Behind a window or shaded, divide by 3-5. A 300 mAh cell bridges about 4 days of no sun
at 3 mA.

Panel selection:

- The solar input maximum is 6 V. A "6 V" panel often has an open-circuit voltage of
  7-8 V; choose a 5 V panel with open-circuit voltage of 6 V or less.
- The charger caps at 540 mA, so panels above about 3 W at 5.5 V only help in dim light.

### Firmware targets

Average current is roughly:

`I_avg = I_sleep + (listen duty x ~5 mA) + (airtime / beacon period x ~200 mA)`

The 5 mA (SX1262 receiving) and 200 mA (PA transmitting at 10 dBm) are estimates from the
datasheet and typical figures.

| Target | Sleep floor | Radio listening | Beacon | Total |
|---|---|---|---|---|
| 3 mA or less (spring to autumn) | about 1 mA | about 0.2 mA (2% duty) | every 30 s, about 1.6 mA | about 2.8 mA |
| 1 mA or less (December) | about 0.4 mA | about 0.1 mA | every 120 s, about 0.4 mA | about 0.9 mA |

What the firmware needs:

1. **No continuous RX.** Use the SX1262 RX duty-cycle mode (short listen windows between
   sleeps) and ESP32 light sleep, woken by DIO1.
2. **Longer beacon preamble.** The sender's preamble must outlast the receiver's sleep
   interval. A 200 ms sleep needs about 200 preamble symbols at SF7 (about 1 ms each),
   so each beacon is about 0.24 s on air. TX then dominates the budget, not RX, and the
   beacon period is the main lever (every 5 s would cost about 10 mA).
3. **Switch off everything unused:** Vext, OLED, USB-CDC serial and the front end
   between transmissions (`femOff()` in `src/common/heltec_v4.h`).
4. **GNSS.** A powered module (assumed about 25 mA) blows the budget. A module on
   connector pin 6 (always-on 3V3) cannot be switched off by firmware; use pin 7
   (switched by GPIO34, low = on) or unplug it.

### Verdict

This panel with duty-cycled firmware should run the boards from about March to October,
with the battery bridging cloudy spells. Winter needs a bigger panel, or a much longer
beacon period with listening mostly off.

## Unknowns to measure

- **Sleep floor with the front end powered.** The KCT8103L idle current is not known, and
  light-sleep current on this board will be well above the 20 µA deep-sleep figure. If
  the floor is above about 1 mA, December is out of reach with this panel.
- **TX current at 10 dBm** (the 200 mA above is an estimate).
- **Real battery runtime.** Log VBAT (ADC on GPIO1 with GPIO37 high; divider ratio is in
  `heltec-v4-hardware.md`) and time a full cell down to about 3.3 V, or put an ammeter in
  the battery lead.
