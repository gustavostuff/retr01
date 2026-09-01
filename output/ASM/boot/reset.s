WORLD     = $FE30
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
        lda #PPUCTRL_BOOT
        sta PPUCTRL
        jmp boot_stream
