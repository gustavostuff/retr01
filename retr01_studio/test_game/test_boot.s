; retr01 Phase 1 — boot streams palette + start MAP, then VBlank pad poll.
; Gameplay: Studio play.c / emu cart runtime (marker R01P @ $80F0).
; Play table @ $8100: present[8] bitmask, spawn_col, spawn_row.
.setcpu "65C02"
WORLD     = $FE30
SCROLL_X  = $FE02
SCROLL_Y  = $FE03
PPUCTRL   = $FE00
PPUSTATUS = $FE01
PAD0      = $FE60
.segment "CODE"
.org $8000
reset:
        sei
        cld
        ldx #$ff
        txs
        lda #0
        sta WORLD
        sta SCROLL_X
        sta SCROLL_Y
        lda #1
        sta PPUCTRL
; palette + MAP stream patched at export — see prg_phase1.c
main:
        lda PPUSTATUS
        and #$80
        beq main
        lda PAD0
        sta $00FE
        jmp main
.segment "PLAY"
.org $8100
; present mask + spawn filled by exporter
; 20 present screens in cart MAP
.segment "VECTORS"
.org $FFFA
        .word main
        .word reset
        .word main
