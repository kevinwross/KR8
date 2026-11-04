; updown.asm — adjust count with '+' and '-', keeping it in 1..9.
;
; Output:
;   + up  - down
;   5
; Each accepted '+' or '-' prints the new value on its own line.
; Newlines and other keys are ignored while polling.

        MOV B, #banner
banner_loop:
        LD A, [B]
        JZ initial
        OUT A
        INC B
        JMP banner_loop

initial:
        LD A, [count]
        ADD A, #'0'
        OUT A
        MOV A, #'\n'
        OUT A

wait:
        IN A
        JZ wait
        CMP A, #'+'
        JZ plus
        CMP A, #'-'
        JZ minus
        CMP A, #'\n'
        JZ wait
        CMP A, #'\r'
        JZ wait
        JMP wait

plus:
        LD A, [count]
        CMP A, #9
        JZ show
        INC A
        ST [count], A
        JMP show

minus:
        LD A, [count]
        CMP A, #1
        JZ show
        DEC A
        ST [count], A
        JMP show

show:
        LD A, [count]
        ADD A, #'0'
        OUT A
        MOV A, #'\n'
        OUT A
        JMP wait

count:  .byte 5
banner: .asciz "+ up  - down\n"
