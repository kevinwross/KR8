; memory.asm — fill buf[0..7] with 1..8 using [B].
        MOV B, #buf
        MOV A, #1
fill:
        ST [B], A
        INC B
        INC A
        CMP A, #9
        JNZ fill
        HLT

buf:    .space 8
