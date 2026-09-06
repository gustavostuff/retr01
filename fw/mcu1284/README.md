# ATmega1284P firmware stub (soft `$FExx`)

Behavioral AVR-C for the HC573-zero soft ports. Not a full sprite/pad firmware yet.
Build later with `avr-gcc -mmcu=atmega1284p` when the ISP toolchain is ready.

**Contract:** [`hw/md/ATmega1284P.md`](../../hw/md/ATmega1284P.md#soft-fexx-hc573-zero)
**Ownership table:** [`docs/graphics.md`](../../docs/graphics.md#fexx-ownership-hc573-zero)

## Soft ports

| Port | Soft bank field | Strobe (schematic) |
|------|-----------------|--------------------|
| `$FE00` | `ppuctrl` | PD4 shared with FE06 |
| `$FE05` | `raster_ctrl` | PD0 shared with FE08 |
| `$FE06` | `bg0_x` | PD4 |
| `$FE07` | `bg0_y` | PD5 (shares UPLDA SEL with FE02) |
| `$FE08` | `pal_addr` | PD0 |
| `$FE90` | `map_lo` | PD1 |
| `$FE91` | `map_mid` | PD1 |
| `$FE92` | `map_hi` | PD1 |

On MAP mid/hi write, UPLDV also loads `CART_A14`-`A18` in hardware. Keep `map_addr` in sync for `$FE93`.

## Shared SEL hazard

Until CPU address bits reach the 1284 (recommended) or +1 PLD gives unique SELs, shared strobes cannot demux alone. Stub records the last strobe group and expects the host not to collide FE00/FE06 in the same edge without demux.

## Files

| File | Role |
|------|------|
| [`soft_fexx.h`](soft_fexx.h) / [`soft_fexx.c`](soft_fexx.c) | Soft register bank + strobe handlers |
| [`main.c`](main.c) | Idle loop stub calling poll helpers |

Sim mirror: `r01s_atmega1284p_soft_*` in `app/sim/chips/atmega1284p.*`.
