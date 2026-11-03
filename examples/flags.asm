; flags.asm — show a signed comparison branch.
        MOV A, #5
        CMP A, #3
        JGT greater
        MOV A, #'N'
        JMP print
greater:
        MOV A, #'G'
print:
        OUT A
        HLT
