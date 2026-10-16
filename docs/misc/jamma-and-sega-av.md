# JAMMA edge and Sega AV socket

The locked arcade headers stay J5 and J6, with J7 for reset. RGB, CSYNC, composite, and mono audio share **J2**, a Kycon **KMDGX-9S-AS**. This page is the pin map for a cabinet build that puts the arcade harness on one JAMMA finger edge at the bottom of the board, and uses that mini-DIN as the Sega Model 2 socket.

Home pads stay on J3 and J4. Those jacks are not part of either connector.

## JAMMA finger edge

Footprint `Retr01_Lib:JAMMA_56_Edge_P3.96mm`. Pitch is 3.96 mm (0.156 in). Each finger is 2.30 mm by 8.00 mm. Front-copper pads P1 to P28 are the parts side. Back-copper pads S1 to S28 are the solder side. Pin 7 is the key on both sides, so P7 and S7 are missing and the footprint carries the notch.

The finger row is 106.92 mm from P1 to P28. The board edge is 170 mm, so the row fits on the bottom with room at both ends. Pin 1 is at one end of that edge. The mating edge of the footprint is x = 0.

JAMMA buttons are switches to ground. Retr01 arcade inputs are the same, with the series 47 ohm already on those lines. Bit order is the `$7F60` / `$7F61` field.

| Control | P1 finger | P2 finger |
| --- | --- | --- |
| Right | P21 | S21 |
| Left | P20 | S20 |
| Down | P19 | S19 |
| Up | P18 | S18 |
| X | P22 | S22 |
| Y | P23 | S23 |
| Coin | P16 | S16 |
| Start | P17 | S17 |

X is JAMMA button 1. Y is JAMMA button 2. Button 3 (P24 and S24) has no Retr01 control.

Video on this edge is RGBS. HSYNC and VSYNC have no JAMMA pin.

| Signal | Finger | Board net |
| --- | --- | --- |
| Red | P12 | J2 pin 7 |
| Green | S12 | J2 pin 3 |
| Blue | P13 | J2 pin 1 |
| CSYNC | S13 | J2 pin 5 |
| Video ground | P14 | GND |

Composite stays off this edge. It belongs on the Sega socket.

| Rail | Fingers |
| --- | --- |
| +5 V | P3, P4, S3, S4 |
| GND | P1, P2, P14, P27, P28, S1, S2, S27, S28 |

J1 and these +5 V fingers are one rail. A cabinet harness feeds the edge. The barrel jack is the bench feed onto that same rail.

The board has no -5 V rail and no +12 V rail. P5, S5, P6, and S6 stay open. A harness that still carries those voltages does not connect them to copper.

Parts pin 15 is the JAMMA test switch, a contact to ground. That is the finger that can share `RESB` with J7. Service (S14) and tilt (S15) stay open.

The speaker pair is P10 and S10. Retr01 audio is PWM through an RC into J2 pins 6, 8, and 9. That path is not a loudspeaker driver, so P10 and S10 stay open.

Also open: coin counters (P8, S8), lockouts (P9, S9), pin 11 on both sides, and pins 25 and 26 on both sides.

| Pin | Parts side | Solder side |
| --- | --- | --- |
| 1 | GND | GND |
| 2 | GND | GND |
| 3 | +5 V | +5 V |
| 4 | +5 V | +5 V |
| 5 | -5 V, open | -5 V, open |
| 6 | +12 V, open | +12 V, open |
| 7 | key | key |
| 8 | counter 1, open | counter 2, open |
| 9 | lockout 1, open | lockout 2, open |
| 10 | speaker +, open | speaker -, open |
| 11 | open | open |
| 12 | Red | Green |
| 13 | Blue | CSYNC |
| 14 | video GND | service, open |
| 15 | test, `RESB` | tilt, open |
| 16 | P1 coin | P2 coin |
| 17 | P1 start | P2 start |
| 18 | P1 up | P2 up |
| 19 | P1 down | P2 down |
| 20 | P1 left | P2 left |
| 21 | P1 right | P2 right |
| 22 | P1 X | P2 X |
| 23 | P1 Y | P2 Y |
| 24 | button 3, open | button 3, open |
| 25 | open | open |
| 26 | open | open |
| 27 | GND | GND |
| 28 | GND | GND |

## Sega Model 2 socket

**J2** is one female 9-pin mini-DIN. The Kycon **KMDGX-9S-AS** is that shell: right-angle, through-hole, 9 sockets, side-and-back shield, board locks. The shield ties to GND.

Footprint `Retr01_Lib:Kycon_KMDGX-9S-AS`. There is no Mini-DIN-9 symbol. `Connector:Mini-DIN-*` stops at 8 pins and ships with an empty footprint field.

Pin numbers are the Kycon recommended PCB layout with the connector facing up, which is the top view of the bottom-view drawing. Those numbers are also the Genesis 2 / Mega Drive 2 / 32X / Nomad map.

| Pin | Sega signal | Board net |
| --- | --- | --- |
| 1 | Blue | Blue gun (R11) |
| 2 | +5 V | +5 V |
| 3 | Green | Green gun (R10) |
| 4 | Composite | AD724 COMP |
| 5 | Sync | CSYNC |
| 6 | Audio mono | `AUDIO_PWM` after the RC |
| 7 | Red | Red gun (R9) |
| 8 | Stereo left | same net as pin 6 |
| 9 | Stereo right | same net as pin 6 |
| S | Shield | GND |

Pins 8 and 9 carry the one PWM channel, so a stereo cable plays it on both sides. HSYNC and VSYNC are not on this socket. RGBS uses pin 5. Composite picture uses pin 4.

RGB at J2 is already about 0.7 Vpp into 75 ohm (R9, R10, R11). A Genesis RGB cable that adds another 75 ohm in series, plus a coupling capacitor, is built for an unbuffered encoder. That cable does not match these levels. Composite on pin 4 is the AD724 output, already arranged for a 75 ohm load. Pin 2 is the board +5 V rail, the same feed a Genesis cable uses for SCART switching.

## Sources

- JAMMA 56-pin chart (parts side and solder side): https://ftp.pigwa.net/stuff/collections/Atari%20documents/Atari%20arcade%20machines%20manuals%20and%20materials/boardpinouts/jamma.txt
- Finger pitch 3.96 mm, finger width about 2.3 mm: arcade fingerboard practice and the 0.156 in edge-connector pitch
- Sega Genesis 2 mini-DIN 9: https://pinoutguide.com/Game/sega2_pinout.shtml
- Same map, including the shield as ground: https://gamesx.com/wiki/doku.php?id=av%3Agenesisav
- Kycon KMDGX-9S-AS: https://www.digikey.com/en/products/detail/kycon-inc/KMDGX-9S-AS/10246531
