; MAP $7F93 -> VRAM $7F12, 480 bytes. Caller already sought both ports.

.global r01_vram_copy_map
.global r01_vram_fill_zero

.section .text,"ax",@progbits
r01_vram_copy_map:
        ldx #240
1:
        lda $7F93
        sta $7F12
        lda $7F93
        sta $7F12
        dex
        bne 1b
        rts

r01_vram_fill_zero:
        lda #0
        ldx #240
1:
        sta $7F12
        sta $7F12
        dex
        bne 1b
        rts

.global r01_oam_blit_bytes
.global r01_oam_scratch
.global r01_oam_scratch_n
r01_oam_blit_bytes:
        lda #0
        sta $7F20
        ldx r01_oam_scratch_n
        beq 2f
        ldy #0
1:
        lda r01_oam_scratch,y
        sta $7F21
        iny
        lda r01_oam_scratch,y
        sta $7F21
        iny
        lda r01_oam_scratch,y
        sta $7F21
        iny
        lda r01_oam_scratch,y
        sta $7F21
        iny
        dex
        bne 1b
2:
        rts
