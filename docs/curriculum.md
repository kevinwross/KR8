# KR8 Curriculum

A suggested sequence for a first assembly course using KR8. Each lesson fits in
one sitting and uses the debugger to make the machine visible.

## Lesson 1 — The machine

Concepts: 8-bit data, 8-bit addresses, 256 bytes of RAM, the five registers,
the four flags, and the idea that code, data, and stack share one memory.

Activities:

- Open the debugger with `kr8 ui`.
- Read the register strip and the 16×16 memory map.
- Explain `PC` (next instruction), `SP` (next free stack byte), and `FLAGS`.
- Note that `0x00` is `HLT`, so blank memory stops.

## Lesson 2 — A first program

Use `examples/hello.asm`.

- `MOV B, #msg` loads an address into B.
- `LD A, [B]` loads a byte and sets Z/N.
- `OUT A` writes it to the console.
- `INC B`, `JMP loop`, `JZ done`, `HLT`.
- Step both a source line and an instruction; watch B and the memory highlight.

Exercise: print a different string, then print a single character without a
string table.

## Lesson 3 — Data and directives

- `.byte`, `.word`, `.ascii`, `.asciz`, `.space`, `.equ`, `.org`.
- How labels become Code, Data, or Const symbols.
- How the assembler reports code bytes, data bytes, and stack headroom.

Exercise: place a table at a fixed address with `.org`, read it with `[B]`, and
check the image with `kr8 dis --symbols`.

## Lesson 4 — Arithmetic and flags

Use `examples/count.asm` and `examples/flags.asm`.

- `ADD`, `ADC`, `SUB`, `SBC`, `CMP`, `INC`, `DEC`.
- Z, C, N, V; unsigned vs signed comparisons.
- `JZ`, `JNZ`, `JC`, `JNC`, `JN`, `JNN`, `JV`, `JNV`, `JLT`, `JGE`, `JGT`,
  `JLE`.

Exercise: count down from 10; then print `G`, `L`, or `E` after comparing two
values.

## Lesson 5 — Memory and pointers

Use `examples/memory.asm`.

- Direct `[addr]` vs indirect `[B]`.
- Walking a pointer with `INC B`/`DEC B`.
- Writing a loop that fills or copies a buffer.

Exercise: reverse an 8-byte buffer in place using `LD A, [B]` and `ST [B], A`.

## Lesson 6 — The stack and subroutines

Use `examples/puts.asm` and `examples/stack.asm`.

- `PUSH`/`POP`, `CALL`/`RET`, the full-descending stack.
- The shadow call stack in the debugger.
- Why the stack and the program collide if a program is too large.

Exercise: write `puthex` that prints A as two hex digits and call it from a
program.

## Lesson 7 — Console input

Use `examples/echo.asm`.

- `IN` is non-blocking: Z=0 when a byte was read, Z=1 when not.
- The polling pattern `IN A` / `JZ wait`.
- Newline handling with `CMP A, #10`.

Exercise: echo uppercase letters as lowercase (or vice versa) until Enter.

Then load `examples/updown.asm`. It keeps a `count` variable in memory, starts at
5, and uses `+`/`-` to move it within 1..9. Point out the read-modify-write
sequence `LD A, [count]` / `INC A` / `ST [count], A`, the clamp branches, and the
non-blocking polling loop.

Exercise: change the starting count or add a reset key.

## Lesson 8 — Debugging

- Breakpoints: source line, disassembly address, symbol.
- Step Over/In/Out; Run/Break/Restart.
- Editing registers, flags, and memory.
- The F1 reference and hover help.
- Reading the status bar: PC, line, instructions, cycles.

Exercise: start from a broken program, use a breakpoint and register editing to
find the bug, then fix the source and reassemble.

## Lesson 9 — A small project

Suggested projects:

- A decimal printer for 0..99 using repeated subtraction.
- A Fibonacci sequence printer (8-bit wrap is part of the lesson).
- A tiny calculator over single-digit input.
- A memory-dump command that prints 16 bytes in hex.
- A maze or grid walker using `[B]` and a table.

Keep programs under roughly 180 bytes so the stack has room. The assembler's
summary and the debugger's memory map make the budget visible.

## Assessment ideas

- Explain why `LD A, [B]` followed by `JZ` works in KR8 but not in KR16.
- Predict the flags after `CMP A, #5` for several A values.
- Find the stack overflow in a deliberately broken `CALL` program.
- Hand-assemble a five-instruction loop and check it with the disassembler.
- Add a symbol and a source line table to a raw `.bin` by including source.
