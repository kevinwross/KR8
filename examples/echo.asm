; echo.asm — echo characters until Enter (0x0A).
; IN is non-blocking: Z is set when no byte is available.
loop:
        IN A
        JZ loop
        CMP A, #10
        JZ done
        OUT A
        JMP loop
done:
        HLT
