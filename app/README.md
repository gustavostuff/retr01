# Retr01 software package

| Path | Role |
|--|--|
| [`studio/`](studio/) | Authoring app (**build-all**) |
| [`emu/`](emu/) | Cart emulator / Studio Play core (**build-all**) |
| [`sim/`](sim/) | Pin-level board simulator (**build-all**). Default cart: `output/test_2.retr01` |
| [`pld_tests/`](pld_tests/) | Headless ATF22V10 fit + scenarios (galette). Via `./unit-tests`, not build-all |
| [`schematic_generator/`](schematic_generator/) | Skidl netlist generator. Separate Python venv |
| [`common/`](common/) | Shared Host Play helpers |
| [`assets/png/`](assets/png/) | Shared PNG images (UI + docs screenshots) |
| [`assets/other/`](assets/other/) | Fonts, licenses, XCF sources |

CMake project / binary names stay `retr01_studio`, `retr01_emu`, `retr01_sim`.
From the repo root, `./build-all` installs `bin/studio`, `bin/emu`, and `bin/sim`.
`./studio`, `./emu`, and `./sim` only run those binaries. `./unit-tests` builds and runs tests (including `app/pld_tests`).
