# Retr01 Tier A sim

Isolated hardware lab for [docs/bringup/tier-a-video-lab.md](../../../docs/bringup/tier-a-video-lab.md). Discrete ICs share one VIDEO LAB sim group. Parts sit free on the board canvas (no island frame). Method B color bars on the virtual screen (256x240 CRT field, 1x default with overscan).

Engine: [`apps/netlist_sim/`](../../netlist_sim/). Palette SoT: [`apps/common/r01_kit_palette.c`](../../common/r01_kit_palette.c). Board chrome lives in `assets/png/` (`pin.png` for DIP pads, plus unused sim PNGs). Overlay font is Proggy Tiny in `assets/other/`.

## Parts

DOT oscillator, FSC oscillator, Beam X and Beam Y (ATF22V10 shells), AT27C256R kit PROM, AD724 encode gate, LCD sink (256x240 RGBS field), nano protoboard.

Passives from the Tier A lab: 100 nF decoupling, 220 uF bulk, R3G3B2 DAC resistors (4.00k / 2.00k / 1.00k and 75 ohm loads), 33 ohm series on DOT and FSC. Right-click empty board to add more passives or another protoboard.

No CPU, cart, AVRs, VRAM, or Compositor.

## Wire modes

| Mode | Wiring |
| --- | --- |
| Auto | Soft netlist (virtual wires). Color bars without placing jumpers. |
| Manual | No virtual wires. A pin conducts only when its tip sits on a breadboard strip, including strips joined by jumpers or by resistors. Caps occupy holes and do not pass DC. The lab runs once the Auto netlist is on the protoboard. |

The 5 V module drives VIN/EN internally so VDD appears on its pin. In Manual, VDD, GND, clocks, and the rest need breadboard connections. OSC OE# may float (not low). PROM CE#/OE# must be tied low. The LCD is blank until that protoboard netlist encodes.

Pin hover draws a 2-elbow (three H/V segments) from that pin to Auto-net partners on other parts. Pins on the ground net (GND/AGND/DGND, PROM CE#/OE# and unused A[13:6], AD724 SELECT, 75 ohm DAC loads) route to a nearby negative-rail hole on a protoboard (left or right edge). Hovering a protoboard GND rail hole draws the same traces out to those pins. Hovering a positive rail hole draws traces to the 5 V net (VDD/VCC/APOS/DPOS, oscillator OE#, PLD RES#, PROM VPP/PGM#, AD724 ENCD/STND/VSYNC).

Part positions, breadboard jumpers, pan, and Auto/Manual are written to `ui_layout.json` on quit and restored on the next launch.

## Build

```text
cmake -S apps/sim/tier-a -B apps/sim/tier-a/build -DCMAKE_BUILD_TYPE=Release
cmake --build apps/sim/tier-a/build -j
ctest --test-dir apps/sim/tier-a/build --output-on-failure
```

Repo wrappers: `./scripts/build-all.sh` installs `bin/sim-tier-a`. `./scripts/sim-tier-a.sh` runs it.

While running, each UI frame advances a short DOT burst under a wall-clock budget so the window stays live.

## Controls

| Key | Action |
| --- | --- |
| Auto/Manual (top-left) | Toggle soft netlist vs breadboard routing |
| A | Same as the Auto/Manual button |
| J | Toggle jumper placement. Click hole A, then hole B. Wheel cycles color after A, or on a selected jumper. |
| Click a jumper | Select. Drag an end to another hole. Delete/Backspace removes selected jumpers. |
| Click a breadboard | Select. Delete/Backspace removes selected protoboards and their jumpers. |
| X | Clear jumpers |
| Hover a pin | 2-elbow traces to Auto-net partners, or to a protoboard GND rail |
| Hover a GND rail hole | 2-elbow traces to ground-net IC and passive pins |
| Hover a VDD rail hole | 2-elbow traces to 5 V-net IC and passive pins |
| Right-click empty board | Add resistor, cap, oscillator, diode, or breadboard |
| Left-drag | Move a part (Shift-click adds to the selection). ICs and passives snap to holes. |
| Drag empty board | Marquee select |
| Middle-drag / right-drag / wheel | Pan |
| Shift+arrows | Pan |
| R | Rotate selected DIP or breadboard 90 deg CW |
| Double-click screen | Toggle LCD 1x (centered playfield, black overscan) / 2x (fill) |
| Space | Pause / resume |
| . | Single DOT half-step while paused |
| Ctrl+R | Reset |
| Esc | Cancel jumper arm/mode, then quit |
