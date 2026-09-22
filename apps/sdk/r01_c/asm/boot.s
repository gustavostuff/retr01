; Retr01 reset. Concatenated into llvm-mos .init (falls through, no RTS).
; Hardware stack $01FF. C stack is __do_init_stack later in .init.

.section .init.50,"ax",@progbits
.global __do_reset
__do_reset:
        sei
        cld
        ldx #$ff
        txs
        lda #0
        sta $7F30               ; WORLD
        sta $7F02               ; SCROLL_X
        sta $7F03               ; SCROLL_Y
        lda #$07                ; L1 | L0 | SPR
        sta $7F00               ; PPUCTRL
