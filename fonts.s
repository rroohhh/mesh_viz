.section .data
.global noto_sans_ttf
noto_sans_ttf:
.incbin "NotoSans[wdth,wght].ttf"
.global noto_sans_ttf_end
noto_sans_ttf_end:
.byte 0
.global fontawesome_ttf
fontawesome_ttf:
.incbin "fontawesome-webfont.ttf"
.global fontawesome_ttf_end
fontawesome_ttf_end:
.byte 0
