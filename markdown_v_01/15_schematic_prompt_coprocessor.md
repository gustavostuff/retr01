Retr01-A schematic generation prompt (coprocessor v0)

You are a PCB / schematic designer. Emit a complete KiCad 8 (preferred), EasyEDA, or Altium schematic from this file alone.

This brief supersedes 13_pcb_schematic_brief.md. Do not draw a discrete 16-sprite shifter farm. Do not use EOL parts (ATF1508, 6116, Lattice GAL22V10, IDT7130).

Do not wait for missing specs. Draw the whole machine. Ugly-but-wired beats a block diagram. Mark guesses FIXME, but do not omit nets.

Product: Retr01-A arcade motherboard + plug-in cart. 5 V through-hole. W65C02S game CPU, discrete BG PPU, ATmega1284P sprite+input coprocessor, ATmega328P APU, analog RGBS.

Planning IC count: 53 on the motherboard (see section 2). Stay on that list. A 4th ATF22V10CQZ-20PU is the only extra PLD allowed.


0. DELIVERABLES

1. Hierarchical schematic of the motherboard (sheets in section 3).
2. Cartridge schematic, same 40-pin header as the motherboard.
3. BOM: manufacturer PN, package, qty. DIP / through-hole. Socket CPU, SRAMs, ATFs, AVRs, EEPROM.
4. CUPL (or equivalent) for 3x ATF22V10CQZ-20PU (GAL-DEC, GAL-TIM, GAL-PPU). Overflow -> 4th ATF22V10CQZ, not a CPLD.
5. 1284 pin map + firmware notes (what the MCU must do; you do not have to write C).
6. Clock/reset/bus-contention notes. Board about 250 x 160 mm, 4-layer. No HDMI.

Forbidden symbols: empty "FPGA PPU", ATF1504/1508, HM6116, dual-port IDT7130, Lattice GAL22V10, merging APU into the 1284.


1. DESIGN RULES

Through-hole, 5 V. DIP W65C02S, AS6C62256, ATF22V10CQZ, AVR, 74HC.

Wide buses are 74HC. VRAM mux = 4x 74HC157. Line-buffer mux = 2x 74HC157. Data = 74HC245. Latches = 74HC573. Beam = 74HC161.

GALs are decode/timing only. Do not GAL-away 8-bit buses.

Three 32 KB SRAMs. Chip 1 = CPU RAM $0000-$7FFF. Chip 2 = interleaved VRAM. Chip 3 = sprite line buffer (not OAM).

Two PPU/CPU clocks: CPU 8.000 MHz PHI2. Dot about 5.369 MHz (21.47727 MHz divided by 4). Independent. The 1284 also needs a 20 MHz crystal. The 328P needs a 16 MHz crystal. Those crystals are extra parts, not extra ICs in the 53.

Sprites: 1284 firmware + ping-pong line buffer. Max 16 per line. Next-line eval.

Input: one byte per player ($FE60, $FE61).

No sprite-0. Raster = Y compare + IRQ. NMI = VBlank only.

No hardware OAM DMA. 6502 loop to $FE21. $FE22 not connected.

Power: barrel 5.5 x 2.1 mm, center positive, 5 V. Polyfuse, reverse diode. Optional unpopulated USB-C 5 V/GND pads (not PD).


2. MOTHERBOARD IC LIST (DRAW THESE)

1x W65C02S6TPG-14 DIP-40 — CPU
1x ATmega328P-PU DIP-28 — APU, 16 MHz crystal
1x ATmega1284P-PU DIP-40 — sprites + pads, 20 MHz crystal
3x AS6C62256-55PCN DIP-28 — sys RAM, VRAM, line buffer
1x AT28C64B-15PU DIP-28 — scores / operator
3x ATF22V10CQZ-20PU DIP-24 — GAL-DEC, GAL-TIM, GAL-PPU
4x 74HC161 DIP-16 — beam X/Y
7x 74HC573 DIP-20 — scroll, banks, OAM capture, pad capture, MAP addr
6x 74HC157 DIP-16 — 4 VRAM addr mux + 2 line-buffer addr mux
3x 74HC245 DIP-20 — CPU / VRAM / cart (or 1284 data isolation)
16x 74HC00/04/08/32/86/688 mix — interleave, HBlank, CHR /CE mux, compositor, 341/262 decode assist
2x oscillators — 8.000 MHz; 21.47727 MHz
1x 5 V regulator module — counted; omit on schematic only if barrel is dedicated 5 V and you note it

Total 53. Sockets, passives, headers, DACs (resistors) are extra, not in 53.


3. SHEETS

00_cover — Title Retr01-A rev B0, block diagram (CPU / BG PPU / 1284 / 328P / cart), 53-chip note
01_power — Barrel, fuse, reverse diode, bulk, 100 nF per IC, LED, USB-C pads
02_cpu — W65C02S, PHI2, RESB Schmitt, RDY/BE/SOB pulled up, NMIB, IRQB
03_decode — GAL-DEC: RAM_CS, IO_CS, PRG_OE, $FExx strobes, EE_CS
04_ram_eeprom — SRAM 1, AT28C64B
05_vram — SRAM 2, 4x 157, 245, PHI2 select
06_io_latches — $FE00-$FE13, $FE30, $FE80, $FE90 latches
07_ppu_timing — divide-by-4 dot clock, 4x 161, GAL-TIM: HBLANK, VBLANK, NMI, raster IRQ
08_ppu_bg — Nametable fetch, attr unpack, BG CHR address, 2bpp shifters
09_sprite_1284 — 1284, SRAM 3, 2x 157, OAM 573, CHR /CE during HBlank, compositor vs BG
10_palette_video — Palette path, R/G/B R-2R, CSYNC, S-Video/composite pads (encoder optional unpopulated)
11_cart — 40-pin header, PRG/CHR/MAP /CE, $FE80, $FE90
12_apu — 328P, $FE40-$FE5F write capture, PWM mix, 3.5 mm jack
13_cabinet — 40-pin IDC to 16 switch lines to 1284 (via 573s). Not four pad bytes.
14_cart_pcb — Flash + same 40-pin


4. CLOCKS

CLK_CPU = 8.000 MHz into PHI2
CLK_DOT = 21.47727 MHz divided by 4, about 5.369 MHz
Line = 341 dots (256 visible + 85 HBlank)
Frame = 262 lines (240 visible + 22 VBlank)
NMI at start of line 240
IRQ when beam_y == RASTER_Y at dot 0, if enable

PHI2 high = CPU owns VRAM port. PHI2 low = BG PPU owns VRAM. Invert with 74HC04 if the 65C02 polarity fights you; document it.


5. CPU MAP (GAL-DEC)

$0000-$7FFF  system RAM
$8000-$FDFF  cart PRG (writes ignored)
$FE00-$FEFF  I/O
$FF00-$FFFF  cart PRG (vectors)

PRG bank only $FE80. off = prg_bank * 0x8000 + (addr & 0x7FFF).

RAM_CS = !A15
IO_CS  = A15 & A14 & A13 & A12 & A11 & A10 & A9 & !A8
PRG_OE = A15 & !IO_CS

W65C02S: VDD +5, 100 nF; PHI2 = CLK_CPU; RESB RC+74HC14; NMIB/IRQB pull-ups 3.3 kOhm; RDY, BE, SOB high.


6. I/O PAGE (USE THESE BYTES)

Family = (addr & 0xFF) >> 4.

$FE00 PPU

$FE00 PPUCTRL — bit7 NMI enable
$FE01 PPUSTATUS — bit7 vblank (clear on read), bit6 raster_hit
$FE02 SCROLL_X
$FE03 SCROLL_Y
$FE04 NT_ARRANGE — 0=1 screen, 1=H, 2=V, 3=2x2; bit2 = plane slots 4-5
$FE05 RASTER_Y
$FE06 BEAM_Y — read-only, low 8 of Y
$FE07 RASTER_IRQ — bit0 enable; write bit7=1 ack hit
$FE08 PAL_ADDR — 0-31
$FE09 PAL_DATA — 6-bit master index

$FE10 VRAM port (PHI2 high only)

VADDR_LO, VADDR_HI (15-bit), VDATA (R/W then addr += VINC), VINC (0 means 1).

$FE20 OAM to 1284 (not a SRAM chip)

$FE20 OAM_ADDR — 8-bit index, 573 or 1284
$FE21 OAM_DATA — write: GAL strobes 1284; auto-inc. 6502 loop 256 stores in VBlank
$FE22 unused, not connected

OAM in 1284 RAM: 64 sprites times (Y, tile, attr, X). Attr: pal bits 1-0, pri bit5, Hflip bit6, Vflip bit7.

$FE30 banks

WORLD bits 2-0, BG_BANK bits 1-0, SPR_BANK bits 1-0. 1284 must snoop WORLD + SPR_BANK (latch copies).

BG CHR: (world*4 + bg_bank)*0x2000 + tile*16 + row + plane*8. Sprite page + 0x1000.

$FE40 APU (328P)

32-byte window. Pulse1, Pulse2, triangle, noise, DMC as NES-like period/volume/duty. DMC may be silence + FIXME. PWM to mix to jack.

$FE60 pads (CPU read-only)

$FE60 = P1
$FE61 = P2

Bit0 Right, bit1 Left, bit2 Down, bit3 Up, bit4 A, bit5 B, bit6 Select/Coin, bit7 Start. 1 = pressed. $FE62 and up unused.

1284 (or 573s it updates) drives these onto the CPU bus when GAL selects $FE60/$FE61.

$FE70 EEPROM

$FE70 data, $FE71 addr-lo, $FE72 addr-hi, $FE73 WE pulse. AT28C64B.

$FE80 PRG_BANK

$FE90 MAP 24-bit

MAP_LO / MAP_MID / MAP_HI, MAP_DATA read auto-inc. MAP not in CPU space.

$FEA0-$FEFF open bus $FF.


7. VRAM (SRAM 2)

$0000-$07FF slot 0: 960 tiles + attrs at +0x3C0
$0800-$1FFF slots 1-3
$2000-$2FFF scratch
$3000-$3FFF plane slots 4-5
$4000-$7FFF wired, unused

Attr: 1 byte = 2x2 tiles, 2 bits each (TL, TR, BL, BR). Not NES shared 2x2.

4x 74HC157 on A0-A14. 245 on D. Never two drivers on D.


8. BG PPU (DISCRETE)

Beam: 161s. X 0..340 wrap; Y 0..261 wrap. GAL: HBLANK (X>=256), VBLANK (Y>=240), DOT0, NMI, raster.

Pixel:

sx = (x + SCROLL_X) & 0xFF
sy = (y + SCROLL_Y) & 0xFF
tile = vram[slot + ty*32 + tx]
attr 2 bits pick which of 4 BG palettes
chr planes from cart BG page
ci = 2bpp
ci==0 -> backdrop; else palette[pal][ci]

NT_ARRANGE picks slots 0-3 (or 4-5 if plane). Shift regs: bank/scroll apply on next tile (up to 8 px). 8x8 tiles only.

CHR /CE during visible = BG PPU. During HBlank = 1284. GAL-PPU makes them exclusive.


9. SPRITE COPROCESSOR (1284) + LINE BUFFER (SRAM 3)

Do not build OAM counters, sprite X 161s, or 16 shifter channels.

Line buffer: SRAM 3, 55 ns. Use 512 bytes.

Bank 0 $0000-$00FF = pixels for the line being drawn
Bank 1 $0100-$01FF = line being filled
Swap at start of HBlank / line

Byte: bits 1-0 color, 3-2 sprite pal, 4 priority, 7-5 unused. Color 0 = transparent.

Visible: 2x 157 select address = {bank_display, BEAM_X[7:0]}. /OE to compositor, /WE high.

HBlank: 157s select 1284 address/data. 1284 /WE pulses.

1284 job (firmware; draw the wiring)

1. Visible: INT on HBLANK falling/rising. Scan 64 OAM Y for next line; keep at most 16.
2. HBlank: take CHR bus; 16x2 reads; blit 8 px each into next bank; rest transparent.
3. VBlank: copy $FE21 writes into OAM[256]; sample pads into $FE60/$FE61 image.

Crystal 20 MHz. ISP: ICSP header (RESET, MOSI, MISO, SCK).

1284 pins (32 GPIO — do not dedicate 16 pins to pads)

Pads fight SRAM address if shared. Lock this split:

8 pins — shared data: CHR / line-buffer D / OAM write latch / pad-latch read
8 pins — line-buffer A0-A7 (only driven in HBlank; Hi-Z visible)
2 pins — /WE_LB, BANK (or leave BANK to GAL)
2 pins — INT0 = HBLANK, INT1 = OAM_WR
2 pins — SEL_P1P2, SEL_OAM (read which 573)
2 pins — WORLD[2:0] can be 573 inputs sampled in software via data bus instead
rest — VBLANK, /CE_CHR sense, unused pulled

Pads: IDC 16 lines to two of the seven 573s (always-transparent or clocked). 1284 reads them on the shared data bus with SEL_P1P2. CPU $FE60/$FE61 = GAL dumps the same 573s (or a 1284-updated pair). Invert so CPU sees 1=pressed. 10 kOhm pull-ups, active-low switches.

OAM write: CPU D + $FE20 addr to 573s; OAM_WR clocks 1284 ISR to OAM[addr]=data.

If you run out of 1284 pins, do not add a 54th IC — drop unused 1284 JTAG/TOSC and keep ICSP. If still short, mux WORLD/SPR_BANK through the existing bank 573s onto the data bus.


10. COMPOSITOR + VIDEO

Per visible pixel:

1. BG ci and sprite line-buffer byte at X.
2. Sprite color 0 -> BG.
3. Else if sprite pri=1 and BG ci != 0 -> BG.
4. Else sprite.

Then 6-bit master index to RGB.

Palette: 32 bytes (8 palettes times 4). $FE08/$FE09. BG palette[n][0] should hardware-mirror to backdrop if easy; else software copies.

LUT speed: AT28C64 (150 ns) is too slow as a pixel LUT (dot about 186 ns, need margin). Put the 64x RGB table in SRAM 3 unused space (e.g. $0200) or 6-bit R-2R from latched palette bytes in the 573 budget. Prefer SRAM 3 $0200-$02FF 6-6-6 packed, GAL second fetch, 55 ns.

R, G, B R-2R, 75 ohm series, about 0.7 V. CSYNC negative. HSYNC in HBlank, VSYNC about Y 244-246. S-Video/composite = pads + unpopulated encoder.

Master colors v0.1 (index 0 = #000000):

#000000 #290514 #2A0507 #230F06 #1E1306 #1A1605 #141807 #061A07 #051A13 #071918 #08181C #071722 #030B3D #16033A #20052D #260420
#363636 #740A40 #77091A #693512 #5D3F0E #514617 #424C19 #13511A #16503F #114E4D #164D58 #164A66 #163794 #472990 #5F167D #6C115F
#949494 #C04A7A #C54A4D #B8601B #A27326 #8F7E2F #77872D #209030 #2E8E72 #318B89 #1F889C #2483B5 #4D77D7 #7E6AD3 #9D5DBF #B352A0
#FFFFFF #F1A2BB #F1A6A1 #F1A983 #EEAC44 #D4BA33 #B0C841 #73D275 #22D0A6 #3BCDC9 #48C9E4 #88C4ED #A4BDEF #BBB5F1 #D5A9EF #F09BDD


11. CART CONNECTOR (40-pin 0.1 inch, keyed)

Pin 1 +5V          Pin 2 +5V
Pin 3 GND          Pin 4 GND
Pins 5-23 A0-A18   Pins 24-31 D0-D7
Pin 32 /OE         Pin 33 /WE (tie high on ROM)
Pin 34 /CE_PRG     Pin 35 /CE_CHR     Pin 36 /CE_MAP
Pin 37 PHI2        Pin 38 /RESET
Pin 39 NC          Pin 40 GND

One /CE at a time. CHR /CE only when !PHI2 or HBlank-1284 as GAL says. MAP /CE on $FE93 read.

Cart PCB: 4x 512 KB 5 V parallel flash DIP-32 (SST39SF040-class if still in stock). If that family is unavailable, one 16 Mbit 5 V/3.3 V TSOP + documented adapter — do not block the motherboard schematic. Regions: PRG 512 KB, CHR 256 KB (half a 512 KB chip), MAP rest.


12. CABINET IDC (40-pin)

16 player switches + power.

P1: Right Left Down Up A B Coin Start. P2 same. Two 573s / $FE60 / $FE61. Remaining IDC: +5V, GND, /RESET, speaker, CSYNC optional. No extra CPU bits for service/tilt.

RGBS on a separate 5-pin header: R G B CSYNC GND.


13. GAL SPLIT

GAL-DEC — RAM_CS, IO_CS, PRG_OE, EE_CS, IO nibble /IO0-9
GAL-TIM — 341/262 with 161 TC, HBLANK, VBLANK, NMI, 688-or-internal raster, IRQB
GAL-PPU — VRAM phase WE/OE, CHR BG vs 1284, LB /OE /WE /BANK, OAM_WR, pad /OE onto CPU D

Overflow -> ATF22V10CQZ number 4, same 5 V DIP.


14. SUCCESS

A human can see: 65C02 + 32 KB RAM + $FExx; VRAM behind 4x 157; 341x262 + NMI; 1284 + SRAM 3 + compositor (no 16-sprite TTL farm); 328P audio; cart PRG/CHR/MAP; $FE60/$FE61 only; RGBS pads.

Video wrong on day one is OK. A box labeled "sprites" with no pins is not.

Draw it now. Use section 2 parts only. Freeze TBDs with this file.
