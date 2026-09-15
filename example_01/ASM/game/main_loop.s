main:
        lda PPUSTATUS
        and #$80
        beq main
        jsr tick_frame
        jsr vblank_frame
        jmp main
