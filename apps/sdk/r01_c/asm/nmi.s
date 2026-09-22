; NMI trampoline and unused IRQ. 65C02 PHX/PHY.

.global nmi
.global irq
.global _irqbrk
.global _irq_default

.section .text.nmi,"axR",@progbits
nmi:
        pha
        phx
        phy
        jsr r01_nmi
        ply
        plx
        pla
        rti

.section .text.irq,"axR",@progbits
_irqbrk:
_irq_default:
irq:
        rti
