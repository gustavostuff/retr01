# Retr01 Tier A sim

Isolated hardware lab for [docs/bringup/tier-a-video-lab.md](../../../docs/bringup/tier-a-video-lab.md). Discrete ICs share one VIDEO LAB sim group. Parts sit free on the board canvas (no island frame). Method B color bars on the virtual screen (256x240 CRT field, 1x default with overscan).

Engine: [`apps/netlist_sim/`](../../netlist_sim/). Palette SoT: [`apps/common/r01_kit_palette.c`](../../common/r01_kit_palette.c). Board chrome lives in `assets/png/` (`pin.png` for DIP pads, plus unused sim PNGs). Overlay font is Proggy Tiny in `assets/other/`.

## Parts

DOT and FSC canned oscillators (4-leg OSC4LEGS sprite, DIP-14 can: pin 14 VDD / 8 clock / 1 OE# / 7 GND), Beam X and Beam Y (ATF22V10 shells), AT27C256R kit PROM, AD724 encode gate, LCD sink (256x240 RGBS field), nano protoboard.

Passives from the Tier A lab: 100 nF decoupling, 220 uF bulk, R3G3B2 DAC resistors (4.00k / 2.00k / 1.00k and 75 ohm loads), 33 ohm series on DOT and FSC. Resistors show 4-band EIA color codes from their ohm value. Electrolytic caps name the pivot lead - and the other lead +. Diodes name the pivot lead A and the other K. Right-click empty board to add more passives or another protoboard.

No CPU, cart, AVRs, VRAM, or Compositor.

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
