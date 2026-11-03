; hello.asm — print a zero-terminated string, then halt.
        MOV B, #msg          ; B points at the text
loop:
        LD A, [B]            ; A = *B; sets Z and N
        JZ done
        OUT A
        INC B
        JMP loop
done:
        HLT

msg:    .asciz "Hello, KR8!\n"
