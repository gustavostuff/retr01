; collision directory: col, row, tab_lo, tab_hi per present screen
        .byte $03, $03, $00, $8E  ; screen (3,3)
        .byte $04, $03, $F0, $8E  ; screen (4,3)
        .byte $05, $03, $E0, $8F  ; screen (5,3)
        .byte $03, $04, $D0, $90  ; screen (3,4)
        .byte $04, $04, $C0, $91  ; screen (4,4)
        .byte $05, $04, $B0, $92  ; screen (5,4)
        .byte $03, $05, $A0, $93  ; screen (3,5)
        .byte $04, $05, $90, $94  ; screen (4,5)
        .byte $05, $05, $80, $95  ; screen (5,5)
play_coll_count: .byte $09
