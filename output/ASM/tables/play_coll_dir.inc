; collision directory: col, row, tab_lo, tab_hi per present screen
        .byte $00, $00, $00, $8E  ; screen (0,0)
        .byte $01, $00, $F0, $8E  ; screen (1,0)
        .byte $02, $00, $E0, $8F  ; screen (2,0)
        .byte $00, $01, $D0, $90  ; screen (0,1)
        .byte $01, $01, $C0, $91  ; screen (1,1)
        .byte $02, $01, $B0, $92  ; screen (2,1)
        .byte $00, $02, $A0, $93  ; screen (0,2)
        .byte $01, $02, $90, $94  ; screen (1,2)
        .byte $02, $02, $80, $95  ; screen (2,2)
        .byte $02, $03, $70, $96  ; screen (2,3)
        .byte $02, $04, $60, $97  ; screen (2,4)
        .byte $03, $04, $50, $98  ; screen (3,4)
play_coll_count: .byte $0C
