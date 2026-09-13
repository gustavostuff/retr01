# Retr01 Overview

**Status:** Draft v1.2  
**Date:** 2026-09-13  
**Architecture:** Dual AVR128DB28 on the main board plus a passive data-only cartridge

Retr01 is an MCU-based 8-bit video game system. It is cartridge based, console ready, and arcade ready.

## Goals

- Classic 8-bit feel with a modern dual-MCU board
- Cheap and safe cartridges that hold data only (no executable code)
- One behaviour model for all games: firmware **contracts** attached to entity types
- Built-in arcade controls and serial pads at the same time
- Uniform **5 V** power domain across the console and controllers when practical

## Board roles

| MCU | Role | Duties |
|-----|------|--------|
| AVR128DB28 #1 | Video | Tile and sprite render, 2x scale, analog video output |
| AVR128DB28 #2 | System | Cartridge access, input, audio, world and entity engine, contracts, saves, SPI master to Video |

## Cartridge model

The cartridge is passive. It has no MCU and no code of any kind (no native code, no bytecode).

It stores:

- World and screen data
- Entity visuals and contract attachments
- Tile banks (CHR)
- Save slots on a separate FRAM chip

All behaviour lives in System MCU firmware as a fixed contract library. Games configure and combine contracts. New mechanics need a firmware update that adds contracts.

## Doc map

| Doc | Contents |
|-----|----------|
| [overview.md](overview.md) | Goals, architecture, contracts idea (this file) |
| [hardware.md](hardware.md) | Power, MCUs, cartridge, video and audio ports, input, inter-MCU link |
| [graphics-world.md](graphics-world.md) | Tiles, sprites, nametable, world and screens |
| [software.md](software.md) | Entities, contracts, cartridge header, boot, frame loop, audio placeholder |

## Design summary

- Dual AVR128DB28 with a clean split of work
- Passive pure-data carts (8 pads per side, 16 contacts total)
- Contracts in firmware are the normal way to build games
- Entity visuals: up to 16 types x 4 states x 4 frames x 4 sprites
- HUD and UI use the same entity system
- Internal 128x120, then 2x to about 256x240
- 1 bpp tiles with an 8-colour attribute
- 32 sprites per frame, 16 per scanline
- RGB + CSYNC and VGA H/V on pin headers, plus composite and mono RCA-style jacks
- Arcade GPIO and two serial controllers always present
- 5 V barrel power (5.5 mm OD / 2.1 mm ID)

## Open items

See the end of [hardware.md](hardware.md) and [software.md](software.md) for remaining TBD items (cart pinout, audio engine, exact resistor or DAC values).
