; stack.asm — preserve A across a subroutine with PUSH/POP.
start:
        MOV A, #'A'
        CALL shout
        OUT A                ; still 'A'
        HLT

shout:
        PUSH A
        MOV A, #'!'
        OUT A
        POP A
        RET
