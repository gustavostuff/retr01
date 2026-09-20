# Retr01 Tier A sim

Isolated hardware lab for [docs/bringup/tier-a-video-lab.md](../../../docs/bringup/tier-a-video-lab.md). Discrete ICs share one VIDEO LAB sim group. Parts sit free on the board canvas (no island frame). Method B color bars on the virtual screen.

Engine: [`apps/netlist_sim/`](../../netlist_sim/). Palette SoT: [`apps/common/r01_kit_palette.c`](../../common/r01_kit_palette.c). Board chrome lives in `assets/png/` (`pin.png` for DIP pads, plus unused sim PNGs). Overlay font is Proggy Tiny in `assets/other/`.

## Parts

DOT oscillator, FSC oscillator, Beam X and Beam Y (ATF22V10 shells), AT27C256R kit PROM, AD724 encode gate, LCD sink (256x240 RGBS field), nano protoboard.

Passives from the Tier A lab: 100 nF decoupling, 220 uF bulk, R3G3B2 DAC resistors (4.00k / 2.00k / 1.00k and 75 ohm loads), 33 ohm series on DOT and FSC.

No CPU, cart, AVRs, VRAM, or Compositor.

## Wire modes

| Mode | Wiring |
| --- | --- |
| Auto | Soft netlist (virtual wires). Color bars without placing jumpers. |
| Manual | No virtual wires. A pin conducts only when its tip sits on a breadboard strip, including strips joined by jumpers or by resistors. Caps occupy holes and do not pass DC. Reproduce the Auto netlist on the protoboard for the lab to run. |

The 5 V module still drives VIN/EN internally so VDD appears on its pin. In Manual, VDD, GND, clocks, and the rest still need breadboard connections. OSC OE# may float (not low). PROM CE#/OE# must be tied low. The LCD is blank until that protoboard netlist encodes.

Hover a pin to pulse it and every other pin on the same Auto net (the intended breadboard partners).

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
| Click two holes (Manual) | Place a jumper |
| X | Clear jumpers |
| Hover a pin | Pulse that pin and Auto-net partners |
| Left-drag | Move a part (Shift-click adds to the selection). ICs and passives snap to holes. |
| Drag empty board | Marquee select |
| Middle-drag / right-drag / wheel | Pan |
| Shift+arrows | Pan |
| R | Rotate selected DIP or breadboard 90 deg CW |
| Double-click screen | Toggle LCD 1x / 2x |
| Space | Pause / resume |
| . | Single DOT half-step while paused |
| Ctrl+R | Reset |
| Esc | Quit |
