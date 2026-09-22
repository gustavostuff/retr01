# C SDK (llvm-mos PRG)

Freestanding C99 for the 32 KB cart PRG. Author file is `game_logic.c`. Build:

```bash
./scripts/fetch-llvm-mos.sh
./apps/sdk/r01_c/build-prg.sh [path/to/game_logic.c] [out.prg]
```

Output is 32768 bytes plus `listing.txt` (C mixed with 6502). Linker script: `ld/retr01.ld`. Boot / NMI: `asm/`.
