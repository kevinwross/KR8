; puts.asm — call a subroutine that prints the string at B.
start:
        MOV B, #msg
        CALL puts
        HLT

puts:
        LD A, [B]
        JZ puts_done
        OUT A
        INC B
        JMP puts
puts_done:
        RET

msg:    .asciz "KR8\n"
