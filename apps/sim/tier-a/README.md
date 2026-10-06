# Retr01 Tier A sim

Isolated hardware lab for [docs/bringup/tier-a-video-lab.md](../../../docs/bringup/tier-a-video-lab.md). The window uses the Tier H board view: a DIGITAL island and an ANALOG island. Method B color bars on the virtual screen (256x240 CRT field).

Engine: [`tools/discrete_ic/`](../../../tools/discrete_ic/). Palette SoT: [`apps/common/r01_kit_palette.c`](../../common/r01_kit_palette.c). Chips, font, and board chrome that both labs use live in [`apps/sim/common/`](../common/). Overlay font is Proggy Tiny.

## Parts

DOT canned oscillator (4-leg OSC4LEGS sprite, DIP-14 can: pin 14 VDD / 8 clock / 1 OE# / 7 GND), Beam X and Beam Y (ATF22V10 shells), AT27C256R kit PROM, **J2** 1x6 male pin header, LCD sink (256x240 RGBS field). Clocks, PLDs, and the PROM sit on DIGITAL. The DAC resistors, J2, and the screen sit on ANALOG.

Passives from the Tier A lab: 100 nF decoupling, 220 uF bulk, R3G3B2 DAC resistors (4.00k / 2.00k / 1.00k and 75 ohm loads), 33 ohm series on DOT. Resistors show 4-band EIA color codes from their ohm value.

No CPU, cart, AVRs, VRAM, or Compositor.

## Bench shopping list

Buy this to build the lab in [docs/bringup/tier-a-video-lab.md](../../../docs/bringup/tier-a-video-lab.md) on a desk. Programmer notes follow [docs/general/hardware.md](../../../docs/general/hardware.md) and [docs/ic_behavior/ATF22V10.md](../../../docs/ic_behavior/ATF22V10.md). Adafruit's UPDI Friend is not used here. It programs the AVRs and the cart later, not these PLDs or the color PROM.

### Tier A, required

Each price is one piece, rough USD, about September 2026. Shipping and tax are extra.

| Qty | Buy | Each | Notes |
| --- | --- | --- | --- |
| 2 | ATF22V10C, PDIP-24, 5 V. Example: **ATF22V10CQZ-20PU** | `~$3` | Beam X and Beam Y. Reprogrammable. Socket them. |
| 2 | 24-pin DIP sockets, 0.3 inch | `~$0.50` | So a bad JEDEC can come out |
| 1 | **AT27C256R-45**, PDIP-28, 600 mil | `~$3` | Color PROM. **OTP.** A wrong image means a new chip. A spare is another `~$3`. |
| 1 | 28-pin DIP socket, 0.6 inch | `~$1` | For the PROM |
| 1 | 5 V canned CMOS oscillator, **5.369318 MHz** | `~$4` | DOT. Not a bare crystal. Less common than 8 MHz / colorburst cans. |
| 1 | **1x6** (2.54 mm) header + RGBS cable or adapter | `~$5` | J2-style R/G/B/CSYNC/GND to monitor or capture card |
| 3 | Solderless breadboards | `~$6` | The wired Tier A sim uses three. The bring-up doc does not lock a count |
| 1 | Jumper-wire set | `~$8` | Solid core, long enough to cross boards |
| 1 | Regulated **5 V** supply, a few hundred mA | `~$8` | Plus a way to land 5 V and GND on one breadboard's rails |
| 7 | **100 nF** ceramic | `~$0.10` | One at each IC and oscillator VCC, plus one at the 5 V entry |
| 1 | **220 µF** electrolytic, rated above 5 V | `~$0.50` | Bulk cap at the 5 V entry. Mark the minus lead |
| 2 | **4.00 kΩ** 1% metal film | `~$0.10` | DAC, R and G MSB |
| 3 | **2.00 kΩ** 1% metal film | `~$0.10` | DAC, R and G mid, B MSB |
| 3 | **1.00 kΩ** 1% metal film | `~$0.10` | DAC, R and G LSB, B LSB |
| 3 | **75.0 Ω** 1% metal film | `~$0.10` | One load to GND per gun |
| 1 | **33 Ω** | `~$0.10` | Series on DOT (optional if edges are clean) |

### Program them from the computer

There is no high-voltage programming path on the Retr01 board. Program both PLDs and the PROM before they go into the lab.

| Chip | On the computer | Hardware | Each |
| --- | --- | --- | --- |
| ATF22V10 | **CUPL** or **WinCUPL** (free) writes the JEDEC fuse file. Then the programmer's host tool loads that file. | Arduino **Uno** or **Nano** (`~$10`) running a GAL programmer such as **Afterburner**, or a **TL866-class** programmer (`~$60`) whose device list includes ATF22V10 | software `~$0` |
| AT27C256R | The PROM programmer's host program (free with the programmer). Image is the 64-color kit, packed R3G3B2, 64 bytes. | A **TL866-class** programmer (`~$60`) that supports **27C256** and its VPP. Afterburner does not do this part. One programmer covers both chips. | `~$60` |

Pre-programmed PLDs and PROM avoid CUPL and programmer hardware. Blank distributor stock will not produce a picture on the bench.

### Tier B, after Tier A locks

| Qty | Buy | Each | Notes |
| --- | --- | --- | --- |
| 1 | Same ATF22V10C, PDIP-24 | `~$3` | Compositor. Third JEDEC. Socket it |
| 1 | 24-pin DIP socket | `~$0.50` | |
| 1 | **100 nF** ceramic | `~$0.10` | On the Compositor VCC pin |

Beam X and Beam Y keep the Tier A JEDEC images only if those images already speak count bits and sync. The Tier B lab moves the color index off Beam X and into the Compositor. Same PROM, DAC, RGBS path, DOT, and three breadboards.

### Rough cost (USD, not a quote)

Single-piece distributor prices, about September 2026. Shipping and tax are extra. CUPL, WinCUPL, and the programmer host software are free downloads.

| Line | About |
| --- | --- |
| Tier A parts: two PLDs, two PROMs (one spare), DOT osc, RGBS header/cable, three breadboards, jumpers, 5 V supply, passives, sockets | **USD 55-85** |
| TL866-class programmer (PROM burn; also programs PLDs). Omit when PLDs and PROM arrive pre-programmed | **USD 50-70** |
| Tier B later: one more PLD, socket, and 100 nF | **about USD 5** |

A first bench that can program its own chips lands around **USD 110-150**. Skip the programmer line if the PLDs and PROM arrive already programmed. A hard-to-find **5.369318 MHz** can move the parts line more than the resistors do.

### Do not buy for Tier A or B

W65C02S, AVR128DB28, AS6C62256, 74HC157 / 573 / 574, SST39SF040, 24C64, ATtiny85, an 8 MHz PHI2 oscillator, Adafruit's UPDI Friend, MCP130 supervisor. A scope is the useful extra for first light.

## Sim

The window is the Tier H board view: two islands, green air wires, no protoboard. **DIGITAL** holds clocks, PLDs, the color PROM, decoupling, the DOT series resistor, and the feedback resistor. **ANALOG** holds the DAC ladder (`R1`-`R11`), the RGBS header, and the screen. Power is one `+5V` net. IC pins on `+5V` are red and GND pins are black. Air wires to the single `5V` and `GND` symbols appear when that part is hovered. A hop past the Tier H length limit blinks red and black.

Placement, pan, zoom, and the air-wire view are written to `island_layout.json` on quit.

## Build

```text
cmake -S apps/sim/tier-a -B apps/sim/tier-a/build -DCMAKE_BUILD_TYPE=Release
cmake --build apps/sim/tier-a/build -j
ctest --test-dir apps/sim/tier-a/build --output-on-failure
```

Repo wrappers: `./scripts/build-all.sh` installs `bin/sim-tier-a`. `./scripts/sim-tier-a.sh` runs it.

While running, each UI frame advances a short DOT burst under a wall-clock budget so the window stays live. Frames per second sit at the top-right.

## Controls

| Key | Action |
| --- | --- |
| Space | Show every green air wire, then hide them until a part is hovered |
| Hover a part | When wires are hidden, draw that part's green air wires. GND stays hidden. The package or pin under the pointer picks up a light green tint |
| Hover an IC pin | `refdes  number  name  level` (Z, L, H, or X) |
| Left-drag | Move a part or an island |
| Shift+click a part | Add or remove it from the selection |
| Shift+drag on an island | Marquee-select parts, then left-drag any of them to move the group |
| Island corner | Resize that island |
| Right-click chip | Rotate |
| R | Rotate selected part |
| Ctrl+wheel | Zoom |
| Shift+arrows / wheel / middle-drag | Pan |
| Double-click screen | Toggle screen scale |
| P | Pause / resume |
| . | Single step while paused |
| Ctrl+R | Reset |
| S | Save placement now |
| Esc | Quit (also saves `island_layout.json`) |
