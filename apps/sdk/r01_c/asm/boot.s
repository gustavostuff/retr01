; Retr01 reset at $8000. Hardware stack $01FF, then JMP C _start ($C800).

.section .boot,"ax",@progbits
.global __do_reset
.global _start
__do_reset:
        sei
        cld
        ldx #$ff
        txs
        lda #0
        sta $7F30               ; WORLD
        sta $7F02               ; SCROLL_X
        sta $7F03               ; SCROLL_Y
        lda #$07                ; L1 | L0 | SPR (NMI armed from C after init)
        sta $7F00               ; PPUCTRL
        jmp _start
