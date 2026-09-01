; collision directory: col, row, tab_lo, tab_hi per present screen
        .byte $03, $03, $00, $8E  ; screen (3,3)
        .byte $04, $03, $F0, $8E  ; screen (4,3)
        .byte $03, $04, $E0, $8F  ; screen (3,4)
        .byte $04, $04, $D0, $90  ; screen (4,4)
play_coll_count: .byte $04
