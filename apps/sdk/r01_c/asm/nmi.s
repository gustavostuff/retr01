; NMI trampoline and unused IRQ. 65C02 PHX/PHY.
; llvm-mos imaginary regs __rc0..__rc31 live at $E0-$FF. C in NMI
; (tracker + r01_game_on_vblank) clobbers them, so the whole block
; is stacked around jsr r01_nmi.

.global nmi
.global irq
.global _irqbrk
.global _irq_default

.section .text.nmi,"axR",@progbits
nmi:
        pha
        phx
        phy
        ldx #0
1:
        lda $e0,x               ; __rc0
        pha
        inx
        cpx #32
        bne 1b
        jsr r01_nmi
        ldx #31
2:
        pla
        sta $e0,x               ; __rc0
        dex
        bpl 2b
        ply
        plx
        pla
        rti

.section .text.irq,"axR",@progbits
_irqbrk:
_irq_default:
irq:
        rti
