# Retr01 Tier A sim

Isolated hardware lab for [docs/bringup/tier-a-video-lab.md](../../../docs/bringup/tier-a-video-lab.md). Discrete ICs share one VIDEO LAB sim group. Parts sit free on the board canvas (no island frame). Method B color bars on the virtual screen (256x240 CRT field, 1x default with overscan).

Engine: [`apps/netlist_sim/`](../../netlist_sim/). Palette SoT: [`apps/common/r01_kit_palette.c`](../../common/r01_kit_palette.c). Chips, font, and board chrome that both labs use live in [`apps/sim/common/`](../common/). Overlay font is Proggy Tiny.

## Parts

DOT and FSC canned oscillators (4-leg OSC4LEGS sprite, DIP-14 can: pin 14 VDD / 8 clock / 1 OE# / 7 GND), Beam X and Beam Y (ATF22V10 shells), AT27C256R kit PROM, AD724 encode gate, LCD sink (256x240 RGBS field), nano protoboard.

Passives from the Tier A lab: 100 nF decoupling, 220 uF bulk, R3G3B2 DAC resistors (4.00k / 2.00k / 1.00k and 75 ohm loads), 33 ohm series on DOT and FSC. Resistors show 4-band EIA color codes from their ohm value. Electrolytic caps name the pivot lead - and the other lead +. Diodes name the pivot lead A and the other K. Right-click empty board to add more passives or another protoboard.

No CPU, cart, AVRs, VRAM, or Compositor.

## Bench shopping list

Buy this to build the lab in [docs/bringup/tier-a-video-lab.md](../../../docs/bringup/tier-a-video-lab.md) on a desk. Programmer notes follow [docs/general/hardware.md](../../../docs/general/hardware.md) and [docs/ic_behavior/ATF22V10.md](../../../docs/ic_behavior/ATF22V10.md). Adafruit's UPDI Friend is not used here. It programs the AVRs and the cart later, not these PLDs or the color PROM.

### Tier A, required

Each price is one piece, rough USD, about September 2026. Shipping and tax are extra.

| Qty | Buy | Each | Notes |
| --- | --- | --- | --- |
| 2 | ATF22V10C, PDIP-24, 5 V. Example: **ATF22V10CQZ-20PU** | ~$3 | Beam X and Beam Y. Reprogrammable. Socket them. |
| 2 | 24-pin DIP sockets, 0.3 inch | ~$0.50 | So a bad JEDEC can come out |
| 1 | **AT27C256R-45**, PDIP-28, 600 mil | ~$3 | Color PROM. **OTP.** A wrong image means a new chip. A spare is another ~$3. |
| 1 | 28-pin DIP socket, 0.6 inch | ~$1 | For the PROM |
| 1 | **AD724**, SOIC-16 | ~$12 | RGB to composite. This one moves around more than the others. |
| 1 | SOIC-16 to DIP-16 adapter | ~$2 | The lab breadboard takes the DIP side |
| 1 | 5 V canned CMOS oscillator, **5.369318 MHz** | ~$4 | DOT. Not a bare crystal. Less common than the colorburst can. |
| 1 | 5 V canned CMOS oscillator, **3.579545 MHz** | ~$2 | FSC, NTSC. PAL lab uses **4.433618 MHz** instead, and AD724 `STND` low |
| 3 | Solderless breadboards | ~$6 | The wired Tier A sim uses three. The bring-up doc does not lock a count |
| 1 | Jumper-wire set | ~$8 | Solid core, long enough to cross boards |
| 1 | Regulated **5 V** supply, a few hundred mA | ~$8 | Plus a way to land 5 V and GND on one breadboard's rails |
| 1 | RCA jack | ~$1 | AD724 `COMP` to the TV |
| 7 | **100 nF** ceramic | ~$0.10 | One at each IC and oscillator VCC, plus one at the 5 V entry |
| 1 | **220 µF** electrolytic, rated above 5 V | ~$0.50 | Bulk cap at the 5 V entry. Mark the minus lead |
| 2 | **4.00 kΩ** 1% metal film | ~$0.10 | DAC, R and G MSB |
| 3 | **2.00 kΩ** 1% metal film | ~$0.10 | DAC, R and G mid, B MSB |
| 3 | **1.00 kΩ** 1% metal film | ~$0.10 | DAC, R and G LSB, B LSB |
| 3 | **75.0 Ω** 1% metal film | ~$0.10 | One load to GND per gun |
| 2 | **33 Ω** | ~$0.10 | Series on DOT and on FSC |

### Program them from the computer

There is no high-voltage programming path on the Retr01 board. Program both PLDs and the PROM before they go into the lab.

| Chip | On the computer | Hardware | Each |
| --- | --- | --- | --- |
| ATF22V10 | **CUPL** or **WinCUPL** (free) writes the JEDEC fuse file. Then the programmer's host tool loads that file. | Arduino **Uno** or **Nano** (~$10) running a GAL programmer such as **Afterburner**, or a **TL866-class** programmer (~$60) whose device list includes ATF22V10 | software $0 |
| AT27C256R | The PROM programmer's host program (free with the programmer). Image is the 64-color kit, packed R3G3B2, 64 bytes. | A **TL866-class** programmer (~$60) that supports **27C256** and its VPP. Afterburner does not do this part. One programmer covers both chips. | ~$60 |

Buy the PLDs and the PROM already programmed if you do not want those tools. Blank distributor stock will not make a picture.

### Tier B, after Tier A locks

| Qty | Buy | Each | Notes |
| --- | --- | --- | --- |
| 1 | Same ATF22V10C, PDIP-24 | ~$3 | Compositor. Third JEDEC. Socket it |
| 1 | 24-pin DIP socket | ~$0.50 | |
| 1 | **100 nF** ceramic | ~$0.10 | On the Compositor VCC pin |

Beam X and Beam Y keep the Tier A JEDEC images only if those images already speak count bits and sync. The Tier B lab moves the color index off Beam X and into the Compositor. Same PROM, DAC, AD724, clocks, and three breadboards.

### Rough cost (USD, not a quote)

Single-piece distributor prices, about September 2026. Shipping and tax are extra. CUPL, WinCUPL, and the programmer host software are free downloads.

| | About |
| --- | --- |
| Tier A parts: two PLDs, two PROMs (one spare), AD724 plus adapter, both oscillators, three breadboards, jumpers, 5 V supply, RCA jack, passives, sockets | **$80-110** |
| TL866-class programmer, only if you do not already have one. This is what burns the PROM. It can also burn the PLDs | **$50-70** |
| Tier B later: one more PLD, socket, and 100 nF | **about $5** |

A first bench that can program its own chips lands around **$140-180**. Skip the programmer line if the PLDs and PROM arrive already programmed. The AD724 and a hard-to-find 5.369318 MHz can move the parts line more than the resistors do.

### Do not buy for Tier A or B

W65C02S, AVR128DB28, AS6C62256, 74HC157 / 573 / 574, SST39SF040, 24C64, ATtiny85, an 8 MHz PHI2 oscillator, Adafruit's UPDI Friend. A **74HC14** only if the canned-oscillator edges are soft. A scope is the useful extra for first light.

## Wire modes

| Mode | Wiring |
| --- | --- |
| Auto | Soft netlist (virtual wires). Color bars without placing jumpers. |
| Manual | No virtual wires. A pin conducts only when its tip sits on a breadboard strip, including strips joined by jumpers or by resistors, also across protoboards. Caps occupy holes and do not pass DC. DIP pads draw gray. A 1 px pulse on each tip: black/green if that pin is on a hole, orange if the part is only partly seated, black/red (faster) if two pins of the same part share a strip or jumper. The lab runs once the Auto netlist is on the protoboard. Manual mode lists netlist faults at the top-left: open Auto links, unseated pins, and hard shorts (5 V to GND, data or clock to a rail, two data nets tied). Jumpers and strips are hard ties. Resistors are not. |

BB1 + and - rails supply 5 V (positive lanes) and GND (negative lanes), top and bottom. The painted gap on a rail is visual. Each + or - lane is one bus. Extra protoboards have none until a jumper reaches a BB1 rail of that polarity. Auto binds VDD/GND on chip pins without breadboard work. In Manual, VDD, GND, clocks, and the rest need breadboard connections, including those BB1 rails. OSC OE# may float (not low). PROM CE#/OE# must be tied low. The LCD is blank until that protoboard netlist encodes.

Pin hover draws a line from that pin to Auto-net partners on other parts. Space toggles those lines between hover only and always on. The line pulses from fully transparent to a net color: red power, green data, cyan clock, black ground. Pins on the ground net (GND/AGND/DGND, PROM CE#/OE# and unused A[13:6], AD724 SELECT, 75 ohm DAC loads) route to a nearby BB1 negative-rail hole. Hovering a GND rail hole on BB1 draws the same lines out to those pins. Hovering a positive rail hole on BB1 draws lines to the 5 V net (VDD/VCC/APOS/DPOS, oscillator OE#, PLD RES#, PROM VPP/PGM#, AD724 ENCD/STND/VSYNC). Extra protoboard rails are isolated and do not show those lines. A line is omitted once that link already exists on a breadboard strip or jumper.

Part positions, breadboard jumpers, pan, zoom, air-wire visibility, and Auto/Manual are written to `ui_layout.json` on quit and restored on the next launch.

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
| Auto/Manual (top-left) | Toggle soft netlist vs breadboard routing |
| A | Same as the Auto/Manual button |
| J | Toggle jumper placement. Click hole A, then hole B, on the same protoboard or two different ones. Wheel cycles color after A, or on a selected jumper. While hole A is armed, a marching-ants 2-elbow line runs from the cursor to the remaining Auto-net destination. |
| Click a jumper | Select. Drag an end to another hole. Drag an elbow to reroute. Delete/Backspace removes selected jumpers. |
| Click a breadboard | Select. Delete/Backspace removes selected protoboards and their jumpers. |
| X | Clear jumpers |
| Hover a pin | Line to Auto-net partners, or to BB1 GND rail. Pulses transparent to red (power), green (data), cyan (clock), or black (ground). Omits links already on a strip or jumper |
| Hover a GND rail hole | On a BB1 negative rail: pulsing black lines to ground-net IC and passive pins still missing a strip or jumper |
| Hover a VDD rail hole | On a BB1 positive rail: pulsing red lines to 5 V-net IC and passive pins still missing a strip or jumper |
| Right-click empty board | Add resistor, cap, oscillator, diode, or breadboard |
| Left-drag | Move a part (Shift-click adds to the selection). ICs and passives snap to holes. |
| Ctrl+drag resistor tip | Stretch that lead along the body axis. The seating pulse sits at the new end. Snaps to holes. |
| Drag empty board | Marquee select |
| Middle-drag / right-drag / wheel | Pan |
| Ctrl+wheel | Integer zoom of the board canvas (1x to 8x). Wheel up zooms in. The board point under the cursor stays put. |
| Shift+arrows | Pan |
| R | Rotate selected DIP or breadboard 90 deg CW |
| Double-click screen | Toggle LCD 1x (centered playfield, black overscan) / 2x (fill) |
| Space | Toggle air wires always on vs hover only |
| Enter / Return | Pause / resume. Same in Manual and Auto. Manual LCD stays blank until the protoboard matches the Auto netlist and encode can lock. |
| . | Single DOT half-step while paused |
| Ctrl+R | Reset |
| Ctrl+Z / Ctrl+Y | Undo / redo board or part moves, resistor lead stretch, jumper create, and deletes |
| Ctrl+F | Toggle fullscreen |
| Esc | Cancel jumper arm/mode, then quit |
