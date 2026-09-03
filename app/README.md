# Retr01 software package

| Path | Role |
|--|--|
| [`studio/`](studio/) | Authoring app |
| [`emu/`](emu/) | Cart emulator (also Studio Play core) |
| [`sim/`](sim/) | Pin-level board simulator. Default cart: `output/test_2.retr01`. USB flasher island is visual only (WIP) |
| [`common/`](common/) | Shared Host Play helpers |
| [`assets/png/`](assets/png/) | Shared PNG images (UI + docs screenshots) |
| [`assets/other/`](assets/other/) | Fonts, licenses, XCF sources |

CMake project / binary names stay `retr01_studio`, `retr01_emu`, `retr01_sim`.
From the repo root, `./build-all` installs `bin/studio`, `bin/emu`, and `bin/sim`.
`./studio`, `./emu`, and `./sim` only run those binaries. `./unit-tests` builds and runs tests.
