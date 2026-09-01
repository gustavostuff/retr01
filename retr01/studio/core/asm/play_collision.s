; Phase 1 player vs BG solid (PRG solid shadow tables, directory @ CPU $810A).
; ZP: $10-$11 px, $12-$13 py (16-bit LE). Returns A=0 ok, A=1 blocked.
.setcpu "65c02"

PLAY_BASE       = $8500
PLAY_PRESENT    = $8100
PLAY_COLL_COUNT = $810A
PLAY_COLL_DIR   = $810B

.segment "CODE"
.org PLAY_BASE

.export play_pos_ok
play_pos_ok:
        lda $11
        bmi blocked
        lda $13
        bmi blocked
        lda $10
        clc
        adc #7
        sta $14
        lda $11
        adc #0
        sta $15
        lda $12
        clc
        adc #7
        sta $16
        lda $13
        adc #0
        sta $17
        jsr present_ok
        bne blocked
        ldx #0
corner_loop:
        txa
        asl
        tay
        lda corners,y
        clc
        adc $10
        sta $18
        lda #0
        adc $11
        sta $19
        lda corners+1,y
        clc
        adc $12
        sta $1a
        lda #0
        adc $13
        sta $1b
        jsr solid_point
        bne blocked
        inx
        cpx #4
        bcc corner_loop
        lda #0
        rts
blocked:
        lda #1
        rts

; A=0 all present screens under AABB, A=1 missing screen.
present_ok:
        lda $10
        sta $24
        lda $11
        sta $25
        jsr div128
        sta $26          ; c0
        lda $14
        sta $24
        lda $15
        sta $25
        jsr div128
        sta $27          ; c1
        lda $12
        sta $24
        lda $13
        sta $25
        jsr div120
        stx $28          ; r0
        lda $16
        sta $24
        lda $17
        sta $25
        jsr div120
        stx $29          ; r1
        ldx $28
@row:
        cpx $29
        bcs @ok
        lda $26
        sta $2a
@col:
        lda $2a
        cmp $27
        bcs @nrow
        tay
        txa
        pha
        lda PLAY_PRESENT,x
        and bit_mask,y
        pla
        tax
        beq @bad
        inc $2a
        jmp @col
@nrow:
        inx
        jmp @row
@ok:
        lda #0
        rts
@bad:
        lda #1
        rts

; $24/$25 = 16-bit LE in, A = value/128
div128:
        ldx #7
@sh:
        lsr $25
        ror $24
        dex
        bne @sh
        lda $24
        and #$07
        rts

; $24/$25 = wy in, X = wy/120
div120:
        ldx #0
@d:
        lda $25
        ora $24
        beq @out
        lda $24
        cmp #120
        bcc @out
        sec
        sbc #120
        sta $24
        lda $25
        sbc #0
        sta $25
        inx
        cpx #8
        bcc @d
@out:
        rts

solid_point:
        lda $19
        lsr
        lsr
        lsr
        lsr
        lsr
        lsr
        lsr
        sta $1c
        lda $18
        sta $24
        lda $19
        sta $25
        jsr div120
        stx $1d
        lda $18
        and #$7f
        lsr
        lsr
        lsr
        sta $1e
        lda $24
        lsr
        lsr
        lsr
        sta $1f
        ldy #0
find_loop:
        sty $22
        lda PLAY_COLL_COUNT
        cmp $22
        bcc not_solid
        tya
        asl
        asl
        tay
        lda PLAY_COLL_DIR,y
        cmp $1c
        bne find_next
        lda PLAY_COLL_DIR+1,y
        cmp $1d
        bne find_next
        lda PLAY_COLL_DIR+2,y
        sta $20
        lda PLAY_COLL_DIR+3,y
        sta $21
        jmp got_tab
find_next:
        ldy $22
        iny
        jmp find_loop
got_tab:
        lda $1f
        asl
        asl
        asl
        asl
        clc
        adc $1e
        tay
        lda ($20),y
        rts
not_solid:
        lda #0
        rts

bit_mask:
        .byte $01,$02,$04,$08,$10,$20,$40,$80
corners:
        .byte 0,0, 7,0, 0,7, 7,7
