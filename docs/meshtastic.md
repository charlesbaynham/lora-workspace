# Meshtastic on the Heltec V4 boards

Research from 2026-11-22. **Nothing here has been flashed or tested on the boards.** Every
claim is marked by where it came from: the Meshtastic docs and source (read first-hand),
Ofcom's IR 2030 (read first-hand), the public node map (queried today), or a third party
(weaker). Settings and commands are given so they can be tried; none of them has been run.

## Short version

- **It works on our hardware.** The V4 is a Meshtastic support-level-1 board and its V4.3 front
  end (KCT8103L, which both our boards have) is auto-detected by the firmware. Pins and GNSS
  wiring in Meshtastic's variant file agree with [`heltec-v4-hardware.md`](heltec-v4-hardware.md).
- **"Public mesh" means two separate things**: radio (other people's nodes hearing yours on the
  default LongFast channel) and internet (the public MQTT server, which is optional and is
  what puts a node on the online maps). Radio needs nodes within range of the flat. MQTT does not.
- **In the UK, use region `EU_868`.** There is no UK region. It uses 869.4 to 869.65 MHz,
  which Ofcom allows at 500 mW ERP with a 10 % duty cycle. Our current firmware sits in a
  different band (868.0 to 868.6 MHz, 25 mW ERP, 1 %), so the two are not radio-compatible.
- **Set the transmit power yourself.** The firmware default for `EU_868` is 27 dBm at the
  antenna port. Ofcom's limit is ERP, so an antenna with more than about 2 dBi puts that over
  the limit. Suggested: 22 dBm to start.
- **We lose our own firmware's features** (the two-way RSSI beacon, the phone web app, BLE
  updates from it) and get Meshtastic's apps in return. Going back is a USB flash.
- **Whether it is worth it depends on how many nodes are near the flat.** The public map shows
  about 26 nodes in a Greater London box and about 450 in a UK-and-Ireland box, only counting
  nodes that report to MQTT. Check the map for the actual street before flashing anything
  ([how](#is-there-a-mesh-near-you)).

## What the boards are, as Meshtastic sees them

| Item | Meshtastic says | Source |
|---|---|---|
| Board | "Heltec LoRa32 V4" (ESP32-S3R2), env `heltec-v4`, hardware model 110 `HELTEC_V4`, `board_level = release`, support level 1 | [variant `platformio.ini`](https://github.com/meshtastic/firmware/blob/develop/variants/esp32s3/heltec_v4/platformio.ini) |
| Not ours | The V4-**R8** (ESP32-S3R8) has a different pinout and its own firmware. Never flash `firmware-heltec-v4-r8-*` or `firmware-heltec-v4-*` onto the wrong one. Ours is the R2 (see `heltec-v4-hardware.md`) | [device page](https://meshtastic.org/docs/hardware/devices/heltec-automation/lora32/) |
| Firmware file | `firmware-heltec-v4-X.X.X.xxxxxxx.bin` (OLED build), 16 MB partition scheme | device page, variant file |
| V4.2 vs V4.3 | One firmware for both. At boot it floats GPIO2 and reads the pull: high means KCT8103L (V4.3), low means GC1109 (V4.2). Support for V4.3 was added by [PR #9753](https://github.com/meshtastic/firmware/pull/9753) (merged 2026-03-04) | `LoRaFEMInterface.cpp`, PR text |
| Radio pins | NSS 8, SCK 9, MOSI 10, MISO 11, RST 12, DIO1 14, BUSY 13, DIO2 as RF switch, TCXO 1.8 V | variant `variant.h`; same as `lora-bringup.md` |
| Front-end pins | GPIO7 power, GPIO2 CSD, GPIO5 CTX (V4.3) or GPIO46 CPS (V4.2) | variant `variant.h`; same as `heltec-v4-hardware.md` |
| TX gain table | KCT8103L: 13 dB up to 13 dBm, falling to 7 dB at 21 dBm in. GC1109: 11 dB, falling to 7 dB. Max out 28 dBm | `LoRaFEMInterface::powerConversion`; same as `lora-bringup.md` |
| GNSS | Configured as L76K, 9600 baud, power gate GPIO34 (active low), reset 42, PPS 41, wake 40, UART on 38/39 | variant `variant.h`; same as `heltec-v4-hardware.md` |
| OLED | SSD1306 driver on I2C SDA 17, SCL 18, reset 21, `VEXT_ENABLE` 36 active low | variant files |

Two independent reads of the Heltec schematic (ours, and the Meshtastic developers') agree on the
pins, which is a useful cross-check. It is a check on the documents, not on our boards.

**Which firmware.** The newest release not marked as a pre-release is `v2.7.26.54e0d8d`
(2026-06-24; its title says "Beta" but it is not flagged pre-release on GitHub). `v2.8.0` to
`v2.8.2` (Aug to Nov 2026) are pre-releases. The online docs are already the 2.8 docs, so a
setting named in them may not exist in 2.7.26. Use 2.7.26 first.
V4.3 boards need firmware from after March 2026; one news article says 2.7.19 does not work
on them ([Prism News](https://www.prismnews.com/hobbies/meshtastic/meshtastic-firmware-debate-spotlights-heltec-v43-power),
secondary source).

## UK radio rules and the right settings

### Region

Meshtastic has no UK region. UK users select `EU_868` and the firmware applies the European
channel plan. From `RadioInterface.cpp` and the LoRa config docs:

| | `EU_868` |
|---|---|
| Band | 869.4 to 869.65 MHz (250 kHz wide) |
| Firmware power limit | 27 dBm |
| Duty cycle | 10 %, over a rolling hour; the node stops sending when it hits it (`lora.override_duty_cycle` is false by default) |
| LongFast frequency | one slot, 869.525 MHz |
| Presets | the nine general ones **minus the three Turbo presets** (500 kHz does not fit the band) |
| Default preset | `LONG_FAST` |

The same band also has two narrower channel plans, `EU_866` (865.6 to 867.6 MHz, lower duty
cycle) and `EU_N_868`. Selecting a preset from one of those flips the region. Ignore them for
a public mesh: public meshes are on `EU_868` LongFast.

### What Ofcom says

From [IR 2030](https://www.ofcom.org.uk/__data/assets/pdf_file/0028/84970/ir-2030.pdf)
(version published 23 October 2026), read directly:

| Entry | Band | Power | Access rule |
|---|---|---|---|
| IR2030/1/19 | 869.40 to 869.65 MHz | **500 mW ERP** | listen-before-talk-type techniques, **or alternatively duty cycle ≤ 10 %** |
| IR2030/1/16 | 868.0 to 868.6 MHz | 25 mW ERP | the same techniques, or alternatively duty cycle ≤ 1 % |

So the Meshtastic band is legal for licence-exempt use at much higher power and duty cycle
than the band our own firmware uses. (Our band is entry 1/16: `lora-bringup.md` already notes
14 dBm and 1 %.) This is a reading of the table, not legal advice.

### Power: set it by hand

ERP is measured relative to a half-wave dipole: **ERP = conducted power + antenna gain (dBi) − 2.15 dB.**
For the 27 dBm ERP limit:

| Antenna gain | Largest conducted power | ERP at the firmware default of 27 dBm |
|---|---|---|
| 0 dBi | 29.15 dBm | 24.85 dBm |
| 2 dBi | 27.15 dBm | 26.85 dBm |
| 3 dBi | 26.15 dBm | 27.85 dBm (over) |
| 5 dBi | 24.15 dBm | 29.85 dBm (over) |

The default is therefore fine for a bare 0 to 2 dBi whip and over the limit for anything with
gain. Heltec quotes 28 ± 1 dBm for the board, and our range log says the "larger" antennas'
model and gain are **not recorded**, so the margin is unknown. Suggestion: `lora.tx_power 22`,
which is plenty for an indoor node and leaves room for 5 dBi and the ± 1 dB. More power
does not help the other direction: the distant node still transmits at its own power.

Firmware detail: the setting is the wanted output **at the antenna port**. The firmware
subtracts the front-end gain from the table above to get the SX1262 setpoint, the same
calculation as `lora-bringup.md`.

### Airtime and the duty cycle

Computed with the standard LoRa airtime formula, LongFast = SF11, BW 250 kHz, CR 4/5, 16-symbol
preamble, plus a 16-byte Meshtastic header (my own calculation; the header size and preamble
are from memory of the protocol, not checked here):

| Packet | On air | Packets per hour at 10 % (360 s) |
|---|---|---|
| short text (20 B) | 0.52 s | about 700 |
| position (25 B) | 0.56 s | about 640 |
| node info (40 B) | 0.68 s | about 530 |
| 100 B text | 1.13 s | about 320 |

Our own beacon is 48 ms at SF7 / BW 125 kHz (`lora-bringup.md`). A LongFast packet is
roughly ten times longer. The limit counts everything the node sends, including relaying
other people's packets, not only its own messages. That is the main reason not to set
a fixed indoor node to a router role.

## Joining the public mesh

### Radio side

1. Region `EU_868`.
2. Preset `LONG_FAST`, the default. Nodes must share region, preset **and** the primary channel
   name (which picks the frequency slot) to hear each other. The map data below shows 158 of 160 UK
   nodes that report a preset are on LongFast.
3. Primary channel: leave it as the default (named `LongFast`, default key). This is what makes it
   public: anyone with a Meshtastic node can read it.
4. Hop limit 3 (default). Leave it.
5. Role `CLIENT`. Do not use `ROUTER`/`REPEATER` indoors: they rebroadcast everything,
   which uses duty cycle, and a bad position makes the mesh worse.
6. Set the node's position by hand to something coarse, not GPS (see Privacy).

### Internet side (optional)

| | |
|---|---|
| Server | `mqtt.meshtastic.org`, username `meshdev`, password `large4cats`. These are published defaults. If the address, username and password are left blank, the node uses them |
| Gateway | A node with WiFi can uplink. **Enabling WiFi disables Bluetooth** ([network docs](https://meshtastic.org/docs/configuration/radio/network/)). The alternative is `mqtt.proxy_to_client_enabled`: the phone app carries the MQTT traffic over its own connection and BLE stays on |
| Per channel | `uplink_enabled` publishes what the node hears. `downlink_enabled` pushes MQTT traffic out to the radio |
| Topic | `msh/EU_868/...` (default root `msh/REGION`) |
| Rules on the public server | Zero-hop policy (MQTT-sourced packets reach the gateway's radio but are not re-relayed). Only a list of port types is forwarded on the default key. Location precision filtered to 10 to 16 bits |
| Advice | The docs recommend **not** using the default LongFast channel for downlink on the public server because of the traffic. Uplink only is the lighter choice |

Uplink is what puts a node on [meshmap.net](https://meshmap.net) and Liam Cottle's map. Neither
is required to talk over radio.

### Privacy

- Position is shared on the default channel. The firmware has a precision setting; map reports
  default to about 1.5 km. A fixed indoor node can use a manually entered position at low
  precision. This matters more than usual here because the boards sit at home.
- Map reporting (`mqtt.map_reporting_enabled`) sends an *unencrypted* report with name, ID, hardware,
  firmware, region, preset and position to the broker.
- `lora.config_ok_to_mqtt` is a polite request, not a technical block, for other people's gateways
  to forward your packets. It is off by default.
- The default key is public. Anything sent on LongFast should be treated as public text.
  Direct messages are end-to-end encrypted to the recipient per the docs, but the routing
  metadata is not.

## Is there a mesh near you?

Meshtastic is only useful for the radio side if other nodes are in range of an indoor antenna.
The repository does not say where the flat is, so I checked UK-wide and for a Greater London
box (51.28 to 51.70 N, 0.52 W to 0.34 E), not for your street.

Source: `https://meshtastic.liamcottle.net/api/v1/nodes` (the data behind
Liam Cottle's public map), fetched 2026-11-22: 24,885 nodes, all with `updated_at` within the
previous 7 days. Positions with a precision below 32 bits are deliberately blurred, so box
membership is approximate. **These are only nodes that uplink to MQTT.** Nodes that do not are
invisible, so these counts are a floor.

| | UK box (49.8 to 60.9 N, 8.7 W to 1.8 E; includes Ireland) | Greater London box |
|---|---|---|
| Nodes with a position | 450 | 26 |
| Seen in last 24 h | 375 | 19 |
| Report region and preset | 160 (118 `EU_868`, 42 `US`) | 9 (all `EU_868`) |
| Preset where known | `LONG_FAST` 158, `LONG_SLOW` 1, `MEDIUM_FAST` 1 | `LONG_FAST` 9 |
| Hardware | Heltec V3 144, **Heltec V4 56**, RAK4631 33, Heltec T114 28 | Heltec V3 10, **Heltec V4 5** |
| Roles | client 314, client_base 69, client_mute 39, router/router_late/router_client 22 | client 21, client_base 4, router_late 1 |
| Firmware where known | 2.7.26 (54), 2.7.15 (24), 2.6.x (11) | 2.7.26 (4), mixed older |

What this tells us:

- LongFast on `EU_868` is the shared setting. Nothing else is common enough to bother with.
- V4 boards are a normal sight on the mesh (56 of 450), so we are not on an odd platform.
- London is thin: 26 mapped nodes across a box of about 60 × 47 km is under one per borough.
  A node indoors on a low floor may hear none. Being near one of the 5 "client base" or router
  nodes in the box would be much better. I did not look up individual positions, and the
  map's own blurring limits what could be learned.

**To check before flashing:** open [meshmap.net](https://meshmap.net) or
[meshtastic.liamcottle.net](https://meshtastic.liamcottle.net/), zoom to the flat's postcode and
count `EU_868` nodes within about 2 km. Zero means a pure radio setup would be a
two-board network that mostly talks to itself.

## What we gain and lose

| | Our firmware (`lora_link`) | Meshtastic |
|---|---|---|
| Frequency / rate | 868.1 MHz, SF7, BW 125 kHz, +10 dBm | 869.525 MHz, SF11, BW 250 kHz (LongFast), up to 27 dBm |
| Receiver sensitivity | SF7, BW 125 kHz | about 7 dB better: SF11 gains roughly 2.5 dB per step over four steps (about 10 dB), the doubled bandwidth costs about 3 dB. An estimate from Meshtastic's rules of thumb; its table gives LongFast a link budget of 153 dB at 22 dBm, i.e. a floor near -131 dBm |
| Talks to others | no | yes, thousands of nodes |
| Two-direction RSSI of our two boards | custom beacon `peer hears us at` | each packet carries RSSI/SNR as heard; apps show them. Traceroute and neighbour info give both directions, but this is **not the same view** as our link page |
| Phone UI | `web/index.html` (Web Bluetooth), our own | Meshtastic apps (Android, iOS), web client at `client.meshtastic.org`, CLI |
| Updates | BLE OTA from our web app | Web Flasher over USB (whether the apps can update a V4 over BLE was not checked) |
| Range test | `docs/range-tests.md` workflow | built-in [Range Test module](https://meshtastic.org/docs/configuration/module/range-test/) (sender every 30 s for LongFast; logs with GPS position; **on a private channel only**, it spams otherwise, and it self-disables after 8 h) |
| Control | everything, 48 ms packets, 1 % duty | enforced 10 % duty cycle, fixed packet format |
| LNA | off | **on by default** on V4.3, with an auto mode in development firmware |

The web app and the `src/` firmware stay in the repo and are not touched by any of
this. Flashing Meshtastic does not delete them, but a board running Meshtastic will not show up in the web
app.

### The LNA default matters for bench testing

`LoRaFEMInterface::init` drives the V4.3 receive LNA **on** at boot. The source comment says
switching it in raises the noise floor by 23 to 25 dB; `lora-bringup.md` measured about 22 dB, so
the two agree. Our boards at 30 cm gave -12 dBm with +10 dBm out and the LNA off. At the 27 dBm
default that becomes about +5 dBm at the receiver before the LNA adds its ~21 dB. `lora-bringup.md`
recalls the SX1262's absolute input maximum as about +10 dBm (not checked against the datasheet).
So for any bench test within a few metres: set the transmit power low (`lora.tx_power 5`) and the
FEM LNA mode to Disabled (`config.lora.fem_lna_mode` in the source; check the exact name in your
app or CLI version). The firmware subtracts the LNA gain from the RSSI it reports.

At the current placement (20 m, 2.5 walls, -58 dBm at +10 dBm and SF7) neither is a problem.
The same link at 22 dBm should read roughly -46 dBm, about 85 dB above LongFast's floor of
-131 dBm. That is a calculation from the old reading, not a measurement.

## Trying it

Not run on hardware. Keep the working firmware recoverable first: it is in the repo, so a
rebuild is enough, but a full read of the flash takes the guesswork out.

```fish
# Back up the whole 16 MB flash of one board (untested; the flash is 16 MB, see heltec-v4-hardware.md)
set port /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_90:DA:72:FB:75:FC-if00
uvx esptool --port $port read-flash 0 0x1000000 backup-FB75FC.bin
```

Flash with the **Web Flasher** (`flasher.meshtastic.org`, needs Chrome or Edge). Pick
"Heltec V4" (not V4-R8), a firmware of 2.7.26 or later, and use the 1200 bps reset button if
the board does not enter download mode. The serial permission rule in the README
(`udev/99-heltec-esp32s3.rules`) applies to the browser too. Choose the full erase and
install option so nothing from our firmware's flash contents is carried over.

Then configure from the CLI (alternatively the app: Settings, LoRa):

```fish
uvx --from meshtastic meshtastic --port $port --set lora.region EU_868 --set lora.modem_preset LONG_FAST --set lora.tx_power 22
uvx --from meshtastic meshtastic --port $port --info
```

Chain `--set` flags in one command, because the board reboots after every command. The CLI
settings are in the [LoRa config page](https://meshtastic.org/docs/configuration/radio/lora/).
Until the region is set the node does not transmit at all.

Suggested order:

1. One board, RF only, region/preset/power as above, no MQTT. Leave it a day by a window. See
   what the node list shows.
2. Both boards flashed, bench test (low power, LNA off) to confirm they hear each other.
3. Add a private secondary channel (custom key, not uplinked) for the two boards' own
   traffic and the Range Test module. Keep the public LongFast channel for the public mesh only.
4. Only then consider MQTT uplink, with client proxy if the BLE connection to a phone matters.

**Going back.** Rebuild and flash with the existing commands in the README
(`pio run -t upload --upload-port <port>`). A first flash of our firmware
over Meshtastic has to go over USB, since our OTA path only exists inside our own firmware.
The backup above is only for the case where the build breaks.

## Open questions

- **Where is the flat?** It sets whether a public mesh is reachable at all.
- **Which antennas?** Gain decides the largest legal power (above). Antenna model and
  gain are not recorded in `range-tests.md`.
- **Two nodes in one flat?** They would be two entries on the mesh, 20 m apart, relaying for
  each other. That helps nobody, and it halves the airtime each could use. It may be cleaner
  to run one on Meshtastic and keep the other for our own firmware experiments.
- **GNSS.** Ours (`FB75FC`) has a module attached; Meshtastic assumes an L76K. Which module it is
  was never identified (`heltec-v4-hardware.md`). Not needed for an indoor node; leave GPS off
  and enter the position by hand.
- **2.7.26 versus 2.8.** The docs describe 2.8 settings (the narrow regions, `fem_lna_mode` auto).
  Not checked which of these 2.7.26 contains.
- **Third-party reports.** One Heltec wiki test reports erratic behaviour and reboots below
  about 3.45 V battery; a MeshCore issue reports the output power flattens above about
  setting 18 to 20. These are single reports, not confirmed, and not checked against our boards.
- **Calculated, not measured.** The 7 dB sensitivity gain, the 85 dB margin, the ERP table and
  the airtimes all come from published parameters and formulas.

## Sources

Read first-hand, 2026-11-22:

- [Meshtastic radio settings](https://meshtastic.org/docs/overview/radio-settings) (EU bands, presets, narrowband)
- [Meshtastic LoRa config](https://meshtastic.org/docs/configuration/radio/lora/) (region table, duty cycle, CLI)
- [Meshtastic MQTT overview](https://meshtastic.org/docs/software/integrations/mqtt) and
  [MQTT module config](https://meshtastic.org/docs/configuration/module/mqtt) (public server, defaults, map reports)
- [Meshtastic network config](https://meshtastic.org/docs/configuration/radio/network/) (WiFi disables Bluetooth)
- [Meshtastic: changes to the public MQTT server, 2024-08-24](https://meshtastic.org/blog/recent-public-mqtt-broker-changes/)
- [Meshtastic Heltec device page](https://meshtastic.org/docs/hardware/devices/heltec-automation/lora32/)
- [Meshtastic Range Test module](https://meshtastic.org/docs/configuration/module/range-test/)
- Meshtastic firmware, `develop` branch: `variants/esp32s3/heltec_v4/{variant.h,platformio.ini}`,
  `src/mesh/{LoRaFEMInterface.cpp,RadioInterface.cpp,SX126xInterface.cpp}`
  ([repository](https://github.com/meshtastic/firmware)); [PR #9753](https://github.com/meshtastic/firmware/pull/9753);
  [release v2.7.26.54e0d8d](https://github.com/meshtastic/firmware/releases/tag/v2.7.26.54e0d8d)
- [Ofcom IR 2030](https://www.ofcom.org.uk/__data/assets/pdf_file/0028/84970/ir-2030.pdf) entries IR2030/1/16 and IR2030/1/19
- [Liam Cottle's Meshtastic map API](https://meshtastic.liamcottle.net/api/v1/nodes) (node counts above)

Secondary or not checked against the original:

- [Prism News on V4.3 and firmware](https://www.prismnews.com/hobbies/meshtastic/meshtastic-firmware-debate-spotlights-heltec-v43-power)
- [Heltec V4 test result](https://wiki.heltec.org/news/heltec-v4-test-result/heltec-v4-test-result) (battery cut-off report)
- [MeshCore issue #1708](https://github.com/meshcore-dev/MeshCore/issues/1708) (V4 TX power measurements)
- UK retailer and community guides saying UK uses `EU_868` and LongFast (e.g.
  [ScotMesh](https://meshtastic.scotmesh.net/)); consistent with Meshtastic's own region table
  and the map data, but not a primary source.
