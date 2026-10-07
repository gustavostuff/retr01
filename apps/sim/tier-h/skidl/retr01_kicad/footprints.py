"""KiCad footprint IDs (from retr01 schematic_generator bom.py)."""

# Retr01_Lib: stock THT geometry with silkscreen trimmed (ref + value only; no fab ${REFERENCE}).
_R = "Retr01_Lib"

DIP28 = f"{_R}:DIP-28_W15.24mm"
DIP28N = f"{_R}:DIP-28_W7.62mm"
DIP32 = f"{_R}:DIP-32_W15.24mm"
DIP40 = f"{_R}:DIP-40_W15.24mm"
DIP24 = f"{_R}:DIP-24_W7.62mm"
DIP20 = f"{_R}:DIP-20_W7.62mm"
DIP16 = f"{_R}:DIP-16_W7.62mm"
SOIC16 = f"{_R}:SOIC-16_3.9x9.9mm_P1.27mm"
DIP14 = f"{_R}:DIP-14_W7.62mm"
DIP14_74HC74 = f"{_R}:DIP-14_W7.62mm_74HC74"
DIP14_74HCU04 = f"{_R}:DIP-14_W7.62mm_74HCU04"
DIP8 = f"{_R}:DIP-8_W7.62mm"

C_CER = f"{_R}:C_Disc_D5.0mm_W2.5mm_P5.00mm"
C_ELEC = f"{_R}:CP_Radial_D8.0mm_P3.50mm"
R_AX = f"{_R}:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical"

OSC8 = f"{_R}:Oscillator_DIP-8"
XTAL = f"{_R}:Crystal_HC49-U_Vertical"
TO92 = f"{_R}:TO-92_Inline"

BARREL = f"{_R}:BarrelJack_GCT_DCJ200-10-A_Horizontal"
HDR2x2 = f"{_R}:PinHeader_2x02_P2.54mm_Vertical"
HDR2x3 = f"{_R}:PinHeader_2x03_P2.54mm_Vertical"
HDR2x4 = f"{_R}:PinHeader_2x04_P2.54mm_Vertical"
HDR6 = f"{_R}:PinHeader_1x06_P2.54mm_Vertical"
HDR10 = f"{_R}:PinHeader_1x10_P2.54mm_Vertical"
HDR2x10 = f"{_R}:PinHeader_2x10_P2.54mm_Vertical"
EDGE36_MOBO = f"{_R}:EDAC_395_MoboSocket_2x18_2.54x5.08mm"
TRS = f"{_R}:Jack_3.5mm_CUI_SJ1-3515N_Horizontal"
# Edge-mount RCJ-01x (1A/1B/1C shell + pad 2 tip); from gametank avboard_tht2.
RCA = "Retr01_Lib:CUI_RCJ-014"
RCA_AUDIO = "Retr01_Lib:CUI_RCJ-014_Audio"
