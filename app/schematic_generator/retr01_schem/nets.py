"""Named nets shared across island modules."""

from __future__ import annotations

# Power
VCC = "+5V"
GND = "GND"

# CPU core
CPU_A = "CPU_A"  # A0-A15 bus prefix
CPU_D = "CPU_D"  # D0-D7
PHI2 = "PHI2"
RESET_N = "RESET_N"
IRQ_N = "IRQ_N"
NMI_N = "NMI_N"

# Clocks
DOT_CLK = "DOT_CLK"

# Decode / I/O
SEL_FE_PREFIX = "SEL_FE"  # SEL_FE02, SEL_FE03, ...

# VRAM
VRAM_A = "VRAM_A"
PPU_VA = "PPU_VA"

# Beam
BEAM_Y = "BEAM_Y"  # Y0-Y7
RASTER_Y = "RASTER_Y"  # FE04 latch Q

# Cart edge (36-pin)
CART_A = "CART_A"
CART_D = "CART_D"
CART_OE_N = "CART_OE_N"
CART_WE_N = "CART_WE_N"
I2C_SDA = "I2C_SDA"
I2C_SCL = "I2C_SCL"

# Video
PROM_IDX = "PROM_IDX"  # compositor -> PROM A5-A0

# Critical nets flagged for Quilter (from AI_flow_for_PCB.pdf)
CRITICAL_NETS = frozenset({PHI2, DOT_CLK, IRQ_N, NMI_N, I2C_SDA, I2C_SCL})
