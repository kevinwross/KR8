; count.asm — print 0 through 9, then a newline.
        MOV B, #10
        MOV A, #'0'
loop:
        OUT A
        INC A
        DEC B
        JNZ loop
        MOV A, #'\n'
        OUT A
        HLT
