; Phase 1 player vs BG solid (PRG solid shadow tables, directory @ CPU $8122).
; ZP: $10-$11 px, $12-$13 py (16-bit LE). Returns A=0 ok, A=1 blocked.
; Present mask: 32 bytes @ $8100 (16 rows x u16 LE). Spawn cell @ $8120.
.setcpu "65c02"

PLAY_BASE       = $8500
PLAY_PRESENT    = $8100
PLAY_SPAWN_CELL = $8120
PLAY_COLL_COUNT = $8121
PLAY_COLL_DIR   = $8122

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
; Uses $26=c0 $27=c1 $28=r0 $29=r1 $2a=col $2b=row $2c=bit
present_ok:
        lda $10
        sta $24
        lda $11
        sta $25
        jsr div128
        sta $26
        lda $14
        sta $24
        lda $15
        sta $25
        jsr div128
        sta $27
        lda $12
        sta $24
        lda $13
        sta $25
        jsr div120
        stx $28
        lda $16
        sta $24
        lda $17
        sta $25
        jsr div120
        stx $29
        lda $28
        sta $2b
@row:
        lda $2b
        cmp $29
        bcs @ok
        lda $26
        sta $2a
@col:
        lda $2a
        cmp $27
        bcs @nrow
        ; byte = PLAY_PRESENT + row*2 + col/8
        lda $2b
        asl
        sta $2c
        lda $2a
        lsr
        lsr
        lsr
        clc
        adc $2c
        tax
        lda $2a
        and #$07
        tay
        lda PLAY_PRESENT,x
        and bit_mask,y
        beq @bad
        inc $2a
        jmp @col
@nrow:
        inc $2b
        jmp @row
@ok:
        lda #0
        rts
@bad:
        lda #1
        rts

; $24/$25 = 16-bit LE in, A = value/128 (0-15)
div128:
        ldx #7
@sh:
        lsr $25
        ror $24
        dex
        bne @sh
        lda $24
        and #$0F
        rts

; $24/$25 = wy in, X = wy/120 (0-15). Remainder left in $24.
div120:
        ldx #0
@d:
        lda $25
        bne @sub
        lda $24
        cmp #120
        bcc @out
@sub:
        lda $24
        sec
        sbc #120
        sta $24
        lda $25
        sbc #0
        sta $25
        inx
        cpx #16
        bcc @d
@out:
        rts

solid_point:
        ; screen col = wx / 128
        lda $18
        sta $24
        lda $19
        sta $25
        jsr div128
        sta $1c
        ; local x tile
        lda $18
        and #$7f
        lsr
        lsr
        lsr
        sta $1e
        ; screen row = wy / 120
        lda $1a
        sta $24
        lda $1b
        sta $25
        jsr div120
        stx $1d
        ; local y tile from remainder
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
