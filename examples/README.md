# KR8 examples

Each example is a small, self-contained program. Assemble and run with:

```text
kr8 as hello.asm
kr8 run hello.k8
```

| File | Teaches | Expected output |
|---|---|---|
| `hello.asm` | `.asciz`, `[B]`, `OUT`, `JZ` on a load | `Hello, KR8!` |
| `count.asm` | `INC`, `DEC`, `JNZ`, character immediates | `0123456789` |
| `echo.asm` | `IN` polling, `CMP`, newline handling | echoes input up to newline |
| `puts.asm` | `CALL`/`RET`, a pointer argument | `KR8` |
| `memory.asm` | `ST [B]`, pointer walking, `CMP` | no output; `buf` = `01..08` |
| `stack.asm` | `PUSH`/`POP` preserving `A` | `!A` |
| `flags.asm` | `CMP`, `JGT` signed branch | `G` |

All examples are deliberately small enough to leave generous stack headroom.
The assembler prints the exact code/data sizes and remaining stack space.
