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
DIP14 = f"{_R}:DIP-14_W7.62mm"
DIP8 = f"{_R}:DIP-8_W7.62mm"

C_CER = f"{_R}:C_Disc_D5.0mm_W2.5mm_P5.00mm"
C_ELEC = f"{_R}:CP_Radial_D8.0mm_P3.50mm"
R_AX = f"{_R}:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical"

OSC8 = f"{_R}:Oscillator_DIP-8"
XTAL = "Crystal:Crystal_HC49-U_Vertical"

BARREL = f"{_R}:BarrelJack_CUI_PJ-063AH_Horizontal"
HDR4 = f"{_R}:PinHeader_1x04_P2.54mm_Vertical"
HDR6 = f"{_R}:PinHeader_1x06_P2.54mm_Vertical"
HDR10 = f"{_R}:PinHeader_1x10_P2.54mm_Vertical"
HDR2x10 = f"{_R}:PinHeader_2x10_P2.54mm_Vertical"
EDGE36_MOBO = f"{_R}:EDAC_395_MoboSocket_2x18_2.54x5.08mm"
TRS = "Retr01_Lib:Jack_3.5mm_Switchcraft_35RAPC2BVN4_Vertical"
# Edge-mount RCJ-01x (1A/1B/1C shell + pad 2 tip); from gametank avboard_tht2.
RCA = "Retr01_Lib:CUI_RCJ-014"
RCA_AUDIO = "Retr01_Lib:CUI_RCJ-014_Audio"
