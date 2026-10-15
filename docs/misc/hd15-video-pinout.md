# DE-15 video port

One female DE-15 (also called HD-15, and often DB-15) can carry RGB, S-video, and composite. VESA defines the RGBHV pinout. S-video and composite on that same shell come from later equipment maps. Two of those maps are in wide use, and they do not agree on which pin is luma, chroma, or composite.

The motherboard video connectors stay the J2 1x8 header and the J9 composite RCA. This page is the pin map for the case where those outputs share one DE-15. Audio stays on J8. The DE-15 video standard has no audio pin.

KiCad symbol `Connector:DE15_Socket_HighDensity`. The board port is a female socket. A right-angle through-hole footprint in the same library is `Connector_Dsub:DSUB-15-HD_Socket_Horizontal_P2.29x2.54mm_EdgePinOffset8.35mm_Housed_MountingHolesOffset10.89mm`. Names that contain `HD` are the 3-row VGA pitch. Plain `DSUB-15` footprints are the wider 2-row connector.

Pin numbers below are the DE-15 numbers. On the female socket, looking into the holes, with the wide side of the D up:

```text
  5  4  3  2  1
   10 9  8  7  6
  15 14 13 12 11
```

## VESA RGBHV

Source: the VESA DDC / VGA connector pinout (IBM VGA, later VESA DDC). RGB is about 0.7 Vpp into 75 ohm. HSYNC and VSYNC are TTL. Pin 13 may carry composite sync instead of HSYNC. That is the RGBS use. Sync-on-green (RGsB) puts sync on pin 2 and is a separate variant.

| Pin | Name | Signal |
| --- | --- | --- |
| 1 | RED | Red video |
| 2 | GREEN | Green video |
| 3 | BLUE | Blue video |
| 4 | ID2 | Reserved (old monitor ID bit 2) |
| 5 | GND | HSYNC return |
| 6 | RED_RTN | Red return |
| 7 | GREEN_RTN | Green return |
| 8 | BLUE_RTN | Blue return |
| 9 | KEY/PWR | +5 V from the source (EDID power). Old VGA keyed this hole |
| 10 | GND | VSYNC and DDC return |
| 11 | ID0 | Reserved (old monitor ID bit 0) |
| 12 | SDA | DDC data (I2C) |
| 13 | HSYNC | Horizontal sync, or composite sync |
| 14 | VSYNC | Vertical sync |
| 15 | SCL | DDC clock (I2C) |

A plain VGA monitor cable uses pins 1, 2, 3, 5, 6, 7, 8, 10, 13, and 14, and often 9, 12, and 15 for DDC.

## S-video and composite on the same shell

Extron (IN1608 and XTP T VGA) and AMX (DXLink multi-format HD-15) publish one shared map. Component sits on the same pins. The source drives one format at a time. A format cable picks its pins. Green, S-video luma, and composite are the same pin, so those three cannot be driven together.

| Pin | RGBHV | Component | S-video | Composite |
| --- | --- | --- | --- | --- |
| 1 | Red | R-Y (Pr) |  |  |
| 2 | Green | Y | Luma (Y) | Composite |
| 3 | Blue | B-Y (Pb) | Chroma (C) |  |
| 4 |  |  |  |  |
| 5 | GND |  |  |  |
| 6 | Red return | R-Y return |  |  |
| 7 | Green return | Y return | Luma return | Composite return |
| 8 | Blue return | B-Y return | Chroma return |  |
| 9 | +5 V |  |  |  |
| 10 | Sync return |  |  |  |
| 11 |  |  |  |  |
| 12 | DDC SDA |  |  |  |
| 13 | HSYNC |  |  |  |
| 14 | VSYNC |  |  |  |
| 15 | DDC SCL |  |  |  |

RGBS on this shell keeps pins 1, 2, and 3 as RGB and puts CSYNC on pin 13. Pin 14 stays open. Extron calls that RGBs. RGBcvS is the same idea with the composite sync on pin 13.

Each analog pair wants its own return. S-video is pins 2 and 7 for luma, pins 3 and 8 for chroma. Composite is pins 2 and 7.

## Analogue cable map

Analogue Nt Mini uses the same shell with a different assignment, plus shorts in the cable that select the mode. This map is not the Extron/AMX map. A cable built for one will swap luma and chroma, or land composite on the wrong pin, on the other.

| Pin | Function |
| --- | --- |
| 1 | Red. Also composite, and component R-Y |
| 2 | Green. Also S-video chroma, and component luma |
| 3 | Blue. Also S-video luma, and component B-Y |
| 4 | Ground |
| 5 | Detection |
| 6 | Return for pin 1 |
| 7 | Return for pin 2 |
| 8 | Return for pin 3 |
| 9 | +5 V |
| 10 | Detection |
| 11 | Ground |
| 12 | No connect |
| 13 | HSYNC, also CSYNC |
| 14 | VSYNC, also CSYNC |
| 15 | Detection |

| Cable | Wired pins | Mode short |
| --- | --- | --- |
| Composite and S-video | 1 composite, 6 composite return, 2 chroma, 7 chroma return, 3 luma, 8 luma return | Pin 4 tied to pin 5 |
| RGB | 1/6 red, 2/7 green, 3/8 blue, 13 HSYNC, 14 VSYNC, 11 sync return | Pin 4 tied to pin 10 |

The older Analogue Nt puts every format on the shell at once: RGB on 1, 2, 3, component on 6, 7, 8, composite on 12, chroma on 15, sync on 13. Pin 9 on that product is +9 V. A VGA display expects +5 V or an open key on pin 9.

## Retr01 signals

RGB comes from the color PROM DAC at about 0.7 Vpp into 75 ohm (R9, R10, R11). J2 carries red, green, blue, CSYNC, HSYNC, and VSYNC. J9 carries AD724 composite. AD724 CRMA (S-video C) and LUMA (S-video Y) stay on the U725 adapter. The encoder drives 75 ohm reverse-terminated loads. Datasheet levels on those pins are about 2.5 Vpp composite, about 2 Vpp luma, and about 1.8 Vpp chroma, before the series resistor.

| Signal | Where it is | DE-15 on the VESA and Extron map |
| --- | --- | --- |
| Red | J2 pin 8 | 1, return 6 |
| Green | J2 pin 6 | 2, return 7 |
| Blue | J2 pin 4 | 3, return 8 |
| HSYNC | J2 pin 5 | 13 |
| VSYNC | J2 pin 7 | 14 |
| CSYNC | J2 pin 3 | 13 when the cable is RGBS |
| GND | J2 pins 1 and 2 | 5, 6, 7, 8, 10 |
| Composite | J9, AD724 COMP | 2, return 7 |
| Luma | U725 adapter, AD724 LUMA | 2, return 7 |
| Chroma | U725 adapter, AD724 CRMA | 3, return 8 |

Pins 2 and 3 are the collision. Green and composite and luma share pin 2. Blue and chroma share pin 3. A VGA monitor cable and a live composite driver cannot use that pin at the same time.

Two published ways through that:

- One format at a time, Extron/AMX and the Analogue Nt Mini shorts. RGBHV or RGBS uses the VESA pins, so a normal VGA cable still works while the port is in RGB mode. Composite and S-video move onto pins 2 and 3 only while that cable is the one fitted. S-video also needs LUMA and CRMA brought off the adapter.
- Every format at once, spare pins, as on the older Analogue Nt. RGB stays on 1, 2, and 3. Composite and chroma need pins a VGA cable does not use for video. Pins 12 and 15 are the DDC pair, and a plain VGA cable ties them to the display EEPROM. Pin 9 is the +5 V DDC supply, not a video pin.

The PROM DAC is RGB, not YPbPr. Component on this shell would be a different analog path.

## Sources

- VGA connector pinout, VESA DDC: https://en.wikipedia.org/wiki/VGA_connector
- Extron IN1608 analog video wiring (RGBHV, RGBs, component, S-video, composite): https://www.manualsdir.com/manuals/311901/extron-electronics-in1608-ma-user-guide-in1608-sa-user-guide-in1606-user-guide-in1608-series-user-guide.html?page=18
- Extron XTP T VGA setup guide (S-video on the G and B pairs, composite on the G pair): https://docslib.org/doc/617380/extron-xtp-t-vga-setup-guide
- AMX DXLink appendix D, HD-15 cable pinout: https://www.manualshelf.com/manual/amx/avb-rx-dxlink-hdmi/instruction-manual-english/page-122.html
- Analogue analog video cables (Nt Mini detection shorts, and the older Nt map): https://www.analogue.co/support/resource/analog-video-cables
