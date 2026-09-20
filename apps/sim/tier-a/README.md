# Retr01 Tier A sim

Isolated hardware lab for [docs/bringup/tier-a-video-lab.md](../../../docs/bringup/tier-a-video-lab.md). Discrete ICs share one VIDEO LAB sim group. Parts sit free on the board canvas (no island frame). Method B color bars on the virtual screen.

Engine: [`apps/netlist_sim/`](../../netlist_sim/). Palette SoT: [`apps/common/r01_kit_palette.c`](../../common/r01_kit_palette.c). Board chrome lives in `assets/png/` (`pin.png` for DIP pads, plus unused sim PNGs). Overlay font is Proggy Tiny in `assets/other/`.

## Parts

DOT oscillator, FSC oscillator, Beam X and Beam Y (ATF22V10 shells), AT27C256R kit PROM, AD724 encode gate, LCD sink (256x240 RGBS field).

No CPU, cart, AVRs, VRAM, or Compositor.

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
| Left-drag | Move a part (Shift-click adds to the selection) |
| Drag empty board | Marquee select |
| Middle-drag / right-drag / wheel | Pan |
| Shift+arrows | Pan |
| R | Rotate selected DIP 90 deg CW |
| Double-click screen | Toggle LCD 1x / 2x |
| Space | Pause / resume |
| . | Single DOT half-step while paused |
| Ctrl+R | Reset |
| Esc | Quit |
