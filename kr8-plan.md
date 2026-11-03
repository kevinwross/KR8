# KR8 — Design and Implementation Plan

Status: accepted for implementation (decisions frozen 2026-11-03);
post-v1 built-in editor extension added 2026-11-03
Implementation checklist: [kr8-impl.md](kr8-impl.md)  
Project: KR8, an 8-bit teaching computer  
Companion project: KR16 (`..\kr16`)  
Deliverables: CPU, assembler, disassembler, VM/CLI, windowed debugger  
Explicitly out of scope: target C/C++ compiler, linker, object files, relocations, DAP/VS Code, memory-mapped I/O

This document is the design contract for the first implementation. `docs/isa.md`
will become the normative ISA reference once code exists; until then this plan is
the source of truth. No KR8 code has been written yet.

---

## 0. Accepted decisions

These are the frozen v1 decisions. The first group was chosen before the plan
was written; the second group was confirmed in the 2026-11-03 review.

| Area | Decision |
|---|---|
| Word size | 8-bit data, 8-bit address, 256 bytes of RAM |
| Architecture | von Neumann: code, data, and stack share one 256-byte RAM |
| Registers | `A`, `B`, `PC`, `SP`, `FLAGS` |
| Flags | `Z` (zero), `C` (carry/borrow), `N` (negative), `V` (signed overflow) |
| Instruction size | Variable: 1 or 2 bytes; the opcode determines the length |
| Branches and calls | Absolute 8-bit targets; no PC-relative displacement, no relocation |
| I/O | `OUT A` sends a byte; `IN A` reads a byte non-blocking and sets `Z` |
| Syntax | KR16-like and orthogonal: `MOV A, #5`, `LD A, [msg]`, `ST [buf], A`, `ADD A, B`, `JZ loop` |
| Image | Self-contained `.k8`: 256-byte RAM, symbols, line table, embedded source, code bitmap, CRC32 |
| Debugger platforms | Windows (Win32 GDI) and Linux (SDL3) together |
| Source editing | Built-in editor in explicit Edit mode (post-v1 extension, 2026-11-03) |
| UI baseline | Port KR16's proven UI skeleton and visual language; replace the C panes with assembly/source panes |
| Host stack | C++20 + CMake; KR16 Win32 GDI and SDL3 backends; static SDL3 on Linux, `/MT` on Windows |
| ISA scope | All 63 instructions, including `TST`, `ADC`/`SBC`, `JV`/`JNV`, and the signed compare helpers |
| `IN` at EOF | EOF is not distinguished from "no byte yet"; `Z=1` in both cases |
| Cycles | The simple documented estimate is retained (bytes + memory access + taken transfer) |
| Next document | `kr8-impl.md`, a task-level work breakdown derived from M0–M6 |

Additional decisions made in this plan:

- `0x00` is `HLT`. Blank or uninitialized RAM therefore stops cleanly instead of
  executing garbage.
- Value-producing instructions (`MOV`, `LD`, `POP`) update `Z` and `N`;
  arithmetic updates `Z`, `C`, `N`, and `V`; `IN` updates `Z` only
  (availability). This deliberately differs from KR16, where loads do not touch
  flags. It makes string loops shorter and more intuitive: `LD A, [B]` followed
  directly by `JZ done`.
- `IN A` is non-blocking at the CPU level: if a byte is available, `A = byte` and
  `Z = 0`; otherwise `A = 0` and `Z = 1`. `Z` is the availability flag, not a
  test of the value. This keeps the VM and the single-threaded UI deterministic.
- The assembler emits a full 256-byte RAM image. A `.bin` export is exactly those
  256 bytes; the `.k8` container adds symbols, line rows, source, and a code map.
- The debugger keeps a shadow call stack. It is authoritative for structured
  `CALL`/`RET` code and best-effort otherwise; the UI labels it accordingly.
- The UI shows the complete 256-byte RAM as a 16x16 grid, not KR16's scrolling
  two-row strip. On this machine the whole address space fits on one screen.
- The UI ports KR16's `Canvas`/`Window`/`Input` interfaces and both platform
  backends first, then replaces `App` with a KR8-specific implementation.

---

## 1. What KR8 is

KR8 is a complete, small teaching computer in the spirit of KR16:

```text
assembly source (hello.asm)
        │  kr8 as
        ▼
self-contained image (hello.k8: RAM + symbols + lines + source)
        │  kr8 run          or          the KR8 window
        ▼
8-bit CPU + 256 bytes of RAM + console
```

It is deliberately not a scaled-down C machine. There is no target C/C++
compiler, no linker, no object format, no relocation, no MMIO, and no
interrupts. Every byte a student sees in RAM got there because the assembler put
it there or because the program wrote it.

The host tools and debugger are C++20, matching KR16's stack so the UI can be
reused; that is the implementation language, not a compiler for the KR8 target.

The toolchain has four pieces:

| Tool | Purpose |
|---|---|
| `kr8 as` | Two-pass assembler: source to `.k8` image plus optional `.bin` and listing |
| `kr8 dis` | Symbol-aware disassembler for `.k8` and raw `.bin` |
| `kr8 run` | Headless VM with console I/O, trace, stats, and fault diagnostics |
| KR8 window | Load/assemble, source and disassembly panes, registers/flags, full memory map, symbols, call stack, console |

### Goals

- A student can understand the entire CPU in one sitting: 5 registers, 63
  instructions, 256 bytes, no hidden state.
- Every instruction is visible and single-steppable; the debugger is exact.
- Assembly source, machine code, symbols, and memory are linked on screen.
- The assembler produces the symbols and line information the debugger needs;
  there is one shared model, not a second interpretation.
- One `.k8` file is a complete artifact: it carries its own symbols and source.
- The UI feels like KR16's debugger: same layout language, same interaction
  grammar, same visual weight, but the content is 8-bit assembly.

### Non-goals for v1

- A target C/C++ compiler, linker, object files, sections, relocations, or macros.
- Interrupts, timers, vectors, privilege modes, or memory-mapped I/O.
- A filesystem, heap, or system-call ABI. Console I/O is `OUT`/`IN`.
- DAP, VS Code integration, or a second debugger front end.
- Conditional breakpoints, watch expressions, reverse execution, or
  multi-threading.
- Multi-file programs, `.include`, and local-label scoping (designed for, but
  deferred).
- 16-bit arithmetic or floating point.

---

## 2. Relationship to KR16

KR16 is the reference for architecture and, more importantly, for the debugger
UI. KR8 reuses the UI skeleton and the design lessons, not the compiler or
linker.

### What we reuse

| KR16 asset | KR8 use |
|---|---|
| `include/kr16/ui/{geometry,canvas,input,window}.hpp` | Same interfaces, renamed `kr8::ui` |
| `src/platform/ui/win32/` GDI canvas, window, dialogs | Windows backend |
| `src/platform/ui/sdl/` SDL3 canvas, window, dialogs | Linux backend |
| `src/ui/text_field.cpp` | Console input and register/memory editors |
| `src/ui/app.cpp` drawing idioms: `fill`, `frame`, `text`, `text_right`, buttons, scrollbars, gutters, tooltips | New KR8 `App` |
| Embedded JetBrains Mono + Inter subsets | Same typography and visual language |
| `cmake/CheckPlatformBoundary.cmake` | Keep OS headers under `src/platform` |
| UI build wiring (`kr8_ui`, smoke target, static SDL3) | Same shape |
| Run-in-slices timer, snapshot rendering, breakpoint gutters, register/flag strip, hover help | Same interaction model |

### What we deliberately do not copy

- The C compiler, libc, linker, container/relocation format, and DAP server.
- KR16's 16-bit CPU, MMIO, BIOS, interrupts, and 64 KiB address space.
- KR16's C-source stepping model. KR8's left pane is assembly source, so source
  lines and instructions are much closer; the dual-granularity idea still holds
  for directives, data lines, and future multi-instruction lines.
- KR16's debug-info defects and workarounds. KR8's assembler emits exact line
  rows and a code map from the start; the debugger does not repair them.

### KR16 to KR8 at a glance

| | KR16 | KR8 |
|---|---|---|
| Data width | 16-bit | 8-bit |
| Address space | 64 KiB | 256 bytes |
| Registers | R0–R4, SP, SR, PC | A, B, SP, FLAGS, PC |
| Instruction size | 2 or 4 bytes | 1 or 2 bytes |
| Branches | PC-relative 8-bit word displacement | Absolute 8-bit target |
| Toolchain | C compiler + assembler + linker | Assembler only |
| Image | `.kex` container with sections/relocations | `.k8` image with symbols/lines/source/code map |
| I/O | MMIO + BIOS + `SYS` | `OUT A` / `IN A` |
| Debugger | C + assembly panes, locals, DAP | Assembly + disassembly panes, symbols, in-process session |

The biggest single simplification: because an address is exactly one byte, a
branch or call target fits in the operand byte. There is no relocation, no
linker, no branch relaxation, and no "out of range" class of assembler error.

---

## 3. The machine

### 3.1 Registers

| Register | Width | Role |
|---|---|---|
| `A` | 8 | Accumulator. The destination of every ALU operation and the only register with arithmetic beyond inc/dec. |
| `B` | 8 | Index / counter. Used for `[B]` addressing, byte pointers, and loop counts. |
| `PC` | 8 | Program counter. Points at the next opcode byte. |
| `SP` | 8 | Stack pointer. Points at the next free byte; initial `0xFF`. |
| `FLAGS` | 8 | `Z`, `C`, `N`, `V` in bits 0–3; bits 7–4 read as zero. |

`A` is the only ALU destination. `B` supports `MOV`, `LD`/`ST`, `INC`, `DEC`,
`PUSH`, and `POP`, but not `ADD B, ...` or `CMP B, ...`. This asymmetry is
intentional: it keeps the opcode set small and makes the accumulator model
obvious. To compare `B`, move it to `A` first; to add to `B`, move it through
`A`.

### 3.2 Flags

| Bit | Name | Meaning |
|---|---|---|
| 0 | `Z` | Zero: the result (or loaded value) is `0x00` |
| 1 | `C` | Carry / no-borrow |
| 2 | `N` | Negative: bit 7 of the result (or loaded value) |
| 3 | `V` | Signed overflow |
| 7–4 | — | Reserved, read as zero |

`C` is carry-out for addition and "no borrow" for subtraction and comparison.
`V` is signed overflow: for addition, both operands have the same sign and the
result has the opposite sign; for subtraction/comparison, the operands have
different signs and the result sign differs from the minuend.

Flag rules:

| Instruction class | Z | C | N | V |
|---|---|---|---|---|
| `ADD`, `ADC`, `SUB`, `SBC`, `CMP`, `NEG`, `INC`, `DEC` | result | carry/borrow | bit 7 | signed overflow |
| `AND`, `OR`, `XOR`, `TST`, `NOT` | result | 0 | bit 7 | 0 |
| `SHL`, `SHR`, `SAR` | result | last bit out | bit 7 | 0 |
| `MOV`, `LD`, `POP` | value | — | value bit 7 | — |
| `IN` | availability (0 = byte read, 1 = none) | — | — | — |
| `ST`, `PUSH`, branches, `CALL`, `RET`, `OUT`, `NOP`, `HLT`, `BRK` | — | — | — | — |
| `CLC` / `SEC` | — | 0 / 1 | — | — |

`MOV`/`LD`/`POP` setting `Z` and `N` is a deliberate KR8 choice. It makes
`LD A, [B]` + `JZ done` the natural string loop and gives beginners immediate
feedback on every loaded value. `C` and `V` are left unchanged.

### 3.3 Memory map

```text
0x00 +-----------------------------+
     | program code                |  entry = image header (usually 0x00)
     | data (.byte/.word/.ascii)   |
     | ...                         |
     | image_high                  |  first unallocated byte (0..256)
     | free RAM                    |
     |                             |
     | stack                       |  grows downward from 0xFF
0xFF +-----------------------------+
```

- RAM is 256 bytes, read/write, unified. There is no ROM and no MMIO.
- `image_high` is the high-water mark of emitted bytes, `0..256`.
- The stack floor is `image_high`. A push that would put `SP` below the floor is
  a stack-overflow fault.
- `SP` starts at `0xFF` and points at the next free byte:
  - `PUSH`: `mem[SP] = value; SP--`.
  - `POP`: `SP++; value = mem[SP]`.
- The CPU tracks an exact stack depth (0..256) in addition to `SP`, so overflow
  and underflow are detected even when the 8-bit `SP` wraps.
- `RET` and `POP` on an empty stack halt with a stack-underflow fault.
- The assembler prints code bytes, data bytes, `image_high`, and stack headroom.
  It warns when headroom is below 16 bytes.

There are no alignment requirements. A 2-byte instruction may start at any
address except `0xFF`; starting there would put the operand at `0x00` after the
PC wraps. The assembler rejects it, and the CPU halts defensively if a raw image
contains it.

### 3.4 Reset state

| State | Value |
|---|---|
| `PC` | Image header `entry` (default: first instruction address) |
| `SP` | `0xFF` |
| `A`, `B` | `0x00` |
| `FLAGS` | `0x00` |
| RAM | The 256-byte image; unwritten bytes are zero |
| Stack depth | 0 |
| Stack floor | `image_high` |
| Halted | false |

`0x00` is `HLT`, so a PC that runs into uninitialized memory stops cleanly.

### 3.5 Instruction encoding

```text
1-byte instruction:   [opcode]
2-byte instruction:   [opcode] [operand]

operand kinds:
  n  = 8-bit immediate
  a  = 8-bit absolute address
  t  = 8-bit absolute branch/call target
```

- The opcode determines whether an operand byte follows. No other width exists.
- `PC` advances by 1 or 2. There is no alignment.
- A 2-byte instruction at `0xFE` is legal: it occupies `0xFE` and `0xFF`, and
  the next `PC` is `0x00`.
- A 2-byte instruction at `0xFF` is illegal: it would fetch its operand from
  `0x100`, which wraps to `0x00`. The assembler rejects it; the CPU faults.
- A 1-byte instruction at `0xFF` is legal and wraps `PC` to `0x00`.
- `CALL` pushes the address of the instruction after the call (one byte). If the
  call is at `0xFE`, the pushed return address is `0x00`.
- `RET` pops one byte into `PC`.

### 3.6 Instruction set

`A` is the accumulator. `B` is the index. `#n` is an 8-bit immediate, `[a]` is a
direct memory address, `[B]` is memory at `B`, and `t` is an absolute target.

#### One-byte instructions

| Opcode | Mnemonic | Flags | Operation |
|---|---|---|---|
| `0x00` | `HLT` | — | Stop the CPU normally |
| `0x01` | `NOP` | — | No operation |
| `0x02` | `BRK` | — | Halt with reason "break"; debugger-resumable |
| `0x03` | `RET` | — | `PC <- pop` |
| `0x04` | `PUSH A` | — | `mem[SP] <- A; SP--` |
| `0x05` | `PUSH B` | — | `mem[SP] <- B; SP--` |
| `0x06` | `POP A` | Z N | `SP++; A <- mem[SP]` |
| `0x07` | `POP B` | Z N | `SP++; B <- mem[SP]` |
| `0x08` | `MOV A, B` | Z N | `A <- B` |
| `0x09` | `MOV B, A` | Z N | `B <- A` |
| `0x12` | `LD A, [B]` | Z N | `A <- mem[B]` |
| `0x15` | `ST [B], A` | — | `mem[B] <- A` |
| `0x20` | `ADD A, B` | Z C N V | `A <- A + B` |
| `0x21` | `ADC A, B` | Z C N V | `A <- A + B + C` |
| `0x22` | `SUB A, B` | Z C N V | `A <- A - B` |
| `0x23` | `SBC A, B` | Z C N V | `A <- A - B - (1 - C)` |
| `0x24` | `CMP A, B` | Z C N V | flags of `A - B`; no write |
| `0x25` | `AND A, B` | Z N | `A <- A & B`; `C=0, V=0` |
| `0x26` | `OR A, B` | Z N | `A <- A \| B`; `C=0, V=0` |
| `0x27` | `XOR A, B` | Z N | `A <- A ^ B`; `C=0, V=0` |
| `0x28` | `TST A, B` | Z N | flags of `A & B`; no write; `C=0, V=0` |
| `0x40` | `NOT A` | Z N | `A <- ~A`; `C=0, V=0` |
| `0x41` | `NEG A` | Z C N V | `A <- 0 - A` |
| `0x42` | `INC A` | Z C N V | `A <- A + 1` |
| `0x43` | `DEC A` | Z C N V | `A <- A - 1` |
| `0x44` | `INC B` | Z C N V | `B <- B + 1` |
| `0x45` | `DEC B` | Z C N V | `B <- B - 1` |
| `0x46` | `SHL A` | Z C N | `C <- A[7]; A <- A << 1` |
| `0x47` | `SHR A` | Z C N | `C <- A[0]; A <- A >> 1` |
| `0x48` | `SAR A` | Z C N | `C <- A[0]; A <- arithmetic A >> 1` |
| `0x49` | `CLC` | C | `C <- 0` |
| `0x4A` | `SEC` | C | `C <- 1` |
| `0x60` | `OUT A` | — | Send `A` to the console |
| `0x61` | `IN A` | Z | If a byte is available: `A <- byte`, `Z <- 0`; else `A <- 0`, `Z <- 1` |

#### Two-byte instructions

| Opcode | Mnemonic | Operand | Flags | Operation |
|---|---|---|---|---|
| `0x0A` | `MOV A, #n` | n | Z N | `A <- n` |
| `0x0B` | `MOV B, #n` | n | Z N | `B <- n` |
| `0x10` | `LD A, [a]` | a | Z N | `A <- mem[a]` |
| `0x11` | `LD B, [a]` | a | Z N | `B <- mem[a]` |
| `0x13` | `ST [a], A` | a | — | `mem[a] <- A` |
| `0x14` | `ST [a], B` | a | — | `mem[a] <- B` |
| `0x30` | `ADD A, #n` | n | Z C N V | `A <- A + n` |
| `0x31` | `ADC A, #n` | n | Z C N V | `A <- A + n + C` |
| `0x32` | `SUB A, #n` | n | Z C N V | `A <- A - n` |
| `0x33` | `SBC A, #n` | n | Z C N V | `A <- A - n - (1 - C)` |
| `0x34` | `CMP A, #n` | n | Z C N V | flags of `A - n`; no write |
| `0x35` | `AND A, #n` | n | Z N | `A <- A & n`; `C=0, V=0` |
| `0x36` | `OR A, #n` | n | Z N | `A <- A \| n`; `C=0, V=0` |
| `0x37` | `XOR A, #n` | n | Z N | `A <- A ^ n`; `C=0, V=0` |
| `0x38` | `TST A, #n` | n | Z N | flags of `A & n`; no write; `C=0, V=0` |
| `0x50` | `JMP t` | t | — | `PC <- t` |
| `0x51` | `JZ t` | t | — | if `Z=1`, `PC <- t` |
| `0x52` | `JNZ t` | t | — | if `Z=0`, `PC <- t` |
| `0x53` | `JC t` | t | — | if `C=1`, `PC <- t` |
| `0x54` | `JNC t` | t | — | if `C=0`, `PC <- t` |
| `0x55` | `JN t` | t | — | if `N=1`, `PC <- t` |
| `0x56` | `JNN t` | t | — | if `N=0`, `PC <- t` |
| `0x57` | `JV t` | t | — | if `V=1`, `PC <- t` |
| `0x58` | `JNV t` | t | — | if `V=0`, `PC <- t` |
| `0x59` | `JLT t` | t | — | if `N != V`, `PC <- t` (signed less) |
| `0x5A` | `JGE t` | t | — | if `N == V`, `PC <- t` (signed greater or equal) |
| `0x5B` | `JGT t` | t | — | if `Z=0 && N == V`, `PC <- t` (signed greater) |
| `0x5C` | `JLE t` | t | — | if `Z=1 \|\| N != V`, `PC <- t` (signed less or equal) |
| `0x5D` | `CALL t` | t | — | push `PC_next`; `PC <- t` |

Opcodes not listed are illegal and halt with an "illegal opcode" fault:
`0x0C–0x0F`, `0x16–0x1F`, `0x29–0x2F`, `0x39–0x3F`, `0x4B–0x4F`,
`0x5E–0x5F`, `0x62–0xFF`. `0x00` being `HLT` is what makes blank memory safe.

#### Branch aliases

The assembler accepts KR16-style aliases, but the disassembler prints the
canonical `Jcc` forms:

| Alias | Canonical |
|---|---|
| `BRA` | `JMP` |
| `BEQ` | `JZ` |
| `BNE` | `JNZ` |
| `BCS`, `BHS` | `JC` |
| `BCC`, `BLO` | `JNC` |
| `BMI` | `JN` |
| `BPL` | `JNN` |
| `BVS` | `JV` |
| `BVC` | `JNV` |
| `BLT` | `JLT` |
| `BGE` | `JGE` |
| `BGT` | `JGT` |
| `BLE` | `JLE` |

Aliases are a convenience for students moving from KR16. New code should use
the canonical names.

### 3.7 Stack, calls, and returns

- `PUSH`/`POP` move one byte and update the exact stack depth.
- `CALL t` pushes the address after the call (one byte) and jumps to `t`.
- `RET` pops one byte into `PC`.
- The CPU detects:
  - **Stack overflow**: a push when depth is 256, or `SP < stack_floor`.
  - **Stack underflow**: a pop or `RET` when depth is 0.
- The debug session also tracks a shadow call stack:
  - `CALL` pushes `{callee, return_address, caller_line}`.
  - `RET` pops one frame.
  - `RET` at call depth 0 is reported as "return without call".
  - This stack drives source-line stepping over/in/out and the Call stack pane.
  - It is exact for structured `CALL`/`RET` code. A subroutine reached by `JMP`
    and left by `RET`, or a hand-built return address, will be reported as
    inconsistent; the UI says "call stack best effort" and restart clears it.

There is no frame pointer and no calling convention beyond "A, B, and flags are
caller-saved; the callee balances the stack". `B` is the natural argument and
pointer register; `A` is the return value.

### 3.8 Console I/O

`OUT A`:

- Sends one raw byte to the host console.
- In the CLI, the byte is written to stdout.
- In the UI, the byte is appended to the Console pane.
- No flags change.

`IN A`:

- Asks the host for one byte, without blocking the CPU.
- If a byte is available: `A = byte`, `Z = 0`. A received `0x00` is a real byte,
  so `Z` is still 0.
- If no byte is available: `A = 0`, `Z = 1`. This is the polling condition.
- `N`, `C`, and `V` are unchanged.
- In the CLI, the host may block on stdin to obtain a byte; at EOF it returns no
  byte forever. In the UI, the host returns a byte from the console input queue
  or no byte immediately. The CPU semantics are identical; only the host's
  waiting policy differs.
- At end of input, `IN` behaves exactly like "no byte yet": `Z=1` forever. A
  program that needs to stop on EOF should use a byte sentinel or the CLI
  instruction budget. This is a deliberate v1 simplification; `C`-as-EOF or an
  `INSTAT` instruction can be added later without changing the image format.
- A polling loop is the intended pattern:

```asm
wait:
    IN A
    JZ wait
```

The UI keeps this responsive because it runs in slices and the console input
queue is filled by the user. A future polish item may pause the run slice after
a long run of empty `IN` polls and show "waiting for input", but that is a UI
convenience, not a change to `IN`.

### 3.9 Faults

| Fault | Cause | PC | Recovery |
|---|---|---|---|
| `IllegalOpcode` | Opcode not in the table | Faulting opcode | Restart |
| `StackOverflow` | Depth 256, or push below `image_high` | Faulting instruction | Restart |
| `StackUnderflow` | Pop/`RET` at depth 0 | Faulting instruction | Restart |
| `PcWrap` | 2-byte instruction at `0xFF` | `0xFF` | Restart |
| `Break` | `BRK` | The `BRK` | Continue/step skips it |
| `Halt` | `HLT` | The instruction after `HLT` | Restart |

`BRK` is a teaching/debugging stop, not a CPU exception. The CPU halts with
`Break` and leaves `PC` on the `BRK`. The session's Continue/Step first advances
`PC` past the `BRK` and clears the halt. The CLI exits 0 at `BRK` with a message.

### 3.10 Cycle model (simple and documented)

KR8 reports a simple cycle estimate, not hardware-accurate timing:

```text
cycles = bytes fetched
       + 1 for each memory access (LD, ST, PUSH, POP, CALL's push, RET's pop)
       + 1 for each taken JMP/Jcc/CALL
```

Examples:

| Instruction | Cycles |
|---|---|
| `ADD A, B` | 1 |
| `ADD A, #n` | 2 |
| `LD A, [B]` | 2 |
| `LD A, [a]` | 3 |
| `JZ t` not taken | 2 |
| `JZ t` taken | 3 |
| `CALL t` | 4 |
| `RET` | 2 |
| `PUSH A` / `POP A` | 2 |
| `OUT A` / `IN A` | 1 |

The exact per-instruction table belongs in `docs/isa.md`. The cycle counter is
shown in the status bar and reported by `kr8 run --stats`; it is optional for
the debugger's core correctness.

---

## 4. Assembler

### 4.1 Source form

```asm
; hello.asm — print a string, then halt.
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
```

Rules:

- One instruction or directive per line.
- `;` starts a comment that runs to end of line.
- Labels are `name:`; a label may share a line with an instruction or directive.
- Labels and `.equ` names are case-sensitive. Mnemonics and directives are
  case-insensitive. `A`/`B` are case-insensitive.
- Whitespace and indentation are free.
- A line may be blank or contain only a comment.
- Operands are separated by commas. `[expr]` and `[B]` are memory; `#expr` is an
  immediate; a bare expression in a branch/call position is an absolute target.

### 4.2 Operand forms

| Form | Meaning | Examples |
|---|---|---|
| `A`, `B` | Register | `MOV A, B`, `INC B` |
| `#expr` | 8-bit immediate | `MOV A, #'A'`, `ADD A, #1` |
| `[expr]` | Direct memory address | `LD A, [msg]`, `ST [buf], A` |
| `[B]` | Memory at `B` | `LD A, [B]`, `ST [B], A` |
| `expr` | Absolute branch/call target | `JMP loop`, `JZ done`, `CALL puts` |

There is no indexed `[B+disp]` and no `[PC+disp]`. With 256 bytes, direct
addresses are always available, and `INC B`/`DEC B` covers pointer walking.

### 4.3 Directives

| Directive | Meaning |
|---|---|
| `.org expr` | Set the location counter to `0..255`; only forward moves allowed |
| `.byte expr, ...` | Emit one byte per expression (`-128..255`, low 8 bits) |
| `.word expr, ...` | Emit two bytes per expression, little-endian |
| `.ascii "text"` | Emit the bytes of `text`, no terminator |
| `.asciz "text"` | Emit the bytes of `text`, then a NUL |
| `.space expr` / `.zero expr` | Reserve `0..256` zero bytes |
| `.entry expr` | Set the image entry address (`0..255`) |
| `.equ NAME, expr` | Define a constant; no storage |

Deferred to a later phase: `.include`, `.align`, macros, local-label scoping,
conditional assembly, and multiple source files. The image format already has a
file index per line row, so multiple files can be added without a format break.

### 4.4 Expressions

- Decimal: `123`
- Hex: `0x7B` or `$7B`
- Binary: `0b01111011`
- Character: `'A'`, with escapes `\n`, `\r`, `\t`, `\0`, `\\`, `\'`, `\"`,
  `\xNN`
- Unary: `+`, `-`, `~`
- Binary: `+`, `-`, `*`, `/`, `%`, `<<`, `>>`, `&`, `^`, `|`
- Parentheses
- Symbols and `.equ` constants

Expressions are evaluated as signed 32-bit integers with C-like precedence.
Range checks happen at the point of use: immediates and addresses must fit
`0..255` (with `-128..-1` accepted and encoded as the low byte where sensible),
and branch/call targets must fit `0..255`. Division or remainder by zero is a
diagnostic. `$` as "current address" is not part of v1; labels are cheap.

### 4.5 Passes

**Pass 1 — parse, size, place**

- Read the source, normalize newlines to LF, and track file/line/column.
- Parse each label, mnemonic, directive, and operand form.
- Instruction size is determined by the mnemonic/operand form, never by a
  symbol value. This is what makes a two-pass assembler without relaxation
  possible.
- Assign addresses to labels and instructions; record data sizes.
- Evaluate `.org`, `.space`, and `.equ` values where they must be known now.
  Forward references in `.org` or `.space` are an error ("requires a value known
  in pass 1").
- Classify labels:
  - A label attached to an instruction is `Code`.
  - A label attached to a data directive is `Data`, with the emitted size.
  - A label with no following instruction or data is an error.
  - `.equ` names are `Const`.
- Reject duplicate labels, duplicate `.equ` names, backward `.org`, and
  instructions that would cross `0xFF`.

**Pass 2 — resolve, encode, emit**

- Evaluate all operand expressions and resolve symbols.
- Encode instructions and data into the 256-byte RAM image.
- Record each instruction start in the 32-byte code bitmap.
- Record one line row per instruction: address, file index, 1-based line.
- Compute symbol values and data sizes.
- Compute `image_high` and stack headroom.
- Emit the `.k8` file, optional `.bin`, optional listing, and summary.

### 4.6 Encoding rules

- `MOV A, #n` / `MOV B, #n`: 2 bytes, opcode + immediate.
- `LD`/`ST` with `[expr]`: 2 bytes, opcode + address.
- `LD`/`ST` with `[B]`: 1 byte.
- ALU with `B`: 1 byte.
- ALU with `#n`: 2 bytes.
- All branches and `CALL`: 2 bytes, opcode + absolute target.
- `JMP`/`Jcc`/`CALL` targets are absolute addresses, not displacements. There is
  no relocation and no range issue beyond the 8-bit address check.
- A 2-byte instruction at `0xFF` is an error ("instruction crosses the end of
  memory; move it or use a 1-byte form").
- A 1-byte instruction at `0xFF` is allowed.

### 4.7 Symbol model

```cpp
enum class SymbolKind : std::uint8_t { Code = 0, Data = 1, Const = 2 };

struct Symbol {
    std::string name;
    std::int32_t value = 0; // address for Code/Data, value for Const
    SymbolKind kind = SymbolKind::Code;
    std::uint16_t size = 0; // Data only; 0 for Code/Const
};
```

- All labels are visible to the debugger. There is no `.global`/`.local`
  distinction because there is no linker.
- `Code` symbols resolve branch/call targets and the "current function" label.
- `Data` symbols resolve `[addr]` operands and give the Symbols pane a size and
  an ASCII preview when printable.
- `Const` symbols are shown in the Symbols pane and usable in expressions.
- The debugger's disassembler prefers a `Code` symbol for a branch target and a
  `Data` symbol for a memory operand. If both exist at one address, the operand
  context picks.

### 4.8 Listing and summary

`kr8 as -l` emits a listing like:

```text
ADDR  BYTES     SOURCE
0000  0B 0A     MOV B, #msg
0002  12        LD A, [B]
0003  51 09     JZ done
0005  60        OUT A
0006  44        INC B
0007  50 02     JMP loop
0009  00        HLT
000A  48 65 6C 6C 6F 2C 20 4B 52 38 21 0A 00   .asciz "Hello, KR8!\n"

Symbols:
  0000 code start
  000A data msg (13 bytes)

Image: 10 code bytes, 13 data bytes, image_high=23, stack headroom=233
```

The exact column widths will be settled in implementation; the information is
the contract. `kr8 as` prints the one-line summary by default; `-q` suppresses
it. The default output is `<source>.k8` next to the source file; `-o` overrides
it.

### 4.9 Diagnostics

Format:

```text
hello.asm:12:9: error: undefined symbol 'done'
   12 |     JZ dnoe
      |        ^
```

- Collect multiple diagnostics; do not write an output file if any error occurs.
- Warnings do not fail unless `-Werror`.
- Required diagnostics:
  - unknown mnemonic or directive
  - invalid operand form
  - missing/extra operand
  - undefined symbol
  - duplicate label / duplicate `.equ`
  - `.org` backwards or outside `0..255`
  - `.space` outside `0..256`
  - immediate/address out of range
  - instruction crosses `0xFF`
  - image exceeds 256 bytes
  - division by zero in an expression
  - unterminated string or invalid escape
  - label with no code or data
  - stack headroom below 16 bytes (warning)
  - entry outside code (warning; allowed for advanced use)
- On error, the previous loaded image (if any) stays in the debugger; the error
  list is shown and the source gutter marks the lines.

### 4.10 Entry point

- If `.entry` is present, it sets the entry address.
- Otherwise the entry is the address of the first instruction emitted.
- If there are no instructions, the assembler errors: there is nothing to run.
- Entry may point into data (advanced), but the assembler warns.

---

## 5. Image format (`.k8`)

The `.k8` file is self-contained: a 256-byte RAM image plus the metadata the
debugger needs. It is little-endian, like the CPU.

### 5.1 Layout

```text
+------------------------------+
| Header (32 bytes)            |
+------------------------------+
| RAM (256 bytes)              |
+------------------------------+
| Code map (32 bytes, 256 bits)|
+------------------------------+
| Symbols (count x 12 bytes)   |
+------------------------------+
| Lines (count x 4 bytes)      |
+------------------------------+
| Sources (count x 12 bytes)   |
+------------------------------+
| String table (u32 size + bytes) |
+------------------------------+
| Source blob (u32 size + bytes)  |
+------------------------------+
```

### 5.2 Header

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | `magic` | `"KR8\0"` |
| 4 | 2 | `version` | `0x0100` for format 1.0 |
| 6 | 2 | `flags` | bit 0 source, bit 1 symbols, bit 2 lines, bit 3 code map |
| 8 | 1 | `entry` | Initial `PC` |
| 9 | 1 | `reserved` | 0 |
| 10 | 2 | `image_high` | High-water mark of emitted bytes, `0..256` |
| 12 | 2 | `symbol_count` | Number of symbol records |
| 14 | 2 | `line_count` | Number of line rows |
| 16 | 2 | `source_count` | Number of source records |
| 18 | 2 | `reserved` | 0 |
| 20 | 4 | `crc32` | CRC32 of everything after the header |
| 24 | 8 | `reserved` | 0 |

### 5.3 Records

**Symbol (12 bytes)**

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | `name_offset` into the string table |
| 2 | 2 | reserved |
| 4 | 4 | `value` (`int32`) |
| 8 | 1 | `kind` (0 code, 1 data, 2 const) |
| 9 | 1 | padding |
| 10 | 2 | `size` (data only; 0 for code/const) |

**Line row (4 bytes)**

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | `address` (instruction start) |
| 1 | 1 | `file` (source index) |
| 2 | 2 | `line` (1-based) |

**Source record (12 bytes)**

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | `path_offset` into the string table |
| 2 | 2 | reserved |
| 4 | 4 | `text_offset` into the source blob |
| 8 | 4 | `text_length` |

### 5.4 Semantics

- `ram[256]` is exactly the machine image. The `.bin` export is this array.
- `code_map` bit `a` is set iff `a` is the first byte of an instruction. Data
  bytes and operand bytes are clear. Address `a` is bit `a & 7` of
  `code_map[a >> 3]` (bit 0 is the least significant bit). The disassembler
  walks set bits; the memory view can distinguish code, operand, and data bytes.
- Line rows are one per instruction. A source line with no instruction (comment,
  label-only line, data line) has no row and is dimmed in the UI.
- Source text is embedded as UTF-8 with LF newlines. The path is informational;
  because the text is embedded, the debugger does not need the original file.
- `image_high` is the stack floor. It is a high-water mark, not a list of
  allocated bytes: a `.org` gap is zero-filled and conservative for the stack.
- The CRC32 uses the standard reflected polynomial `0xEDB88320`. A reader
  rejects a mismatch with "image corrupt or truncated".
- `.k8` is the default output. `--raw out.bin` writes the 256-byte RAM only.
  `--no-source` omits the source blob but keeps symbols and lines.
- A stripped image (no symbols/lines/source) can still run; the debugger shows
  disassembly and memory only.

---

## 6. Disassembler

### 6.1 CLI

```text
kr8 dis hello.k8
kr8 dis hello.k8 --symbols --source
kr8 dis image.bin --raw --base 0x00 --end 0xFF
```

Output:

```text
ADDR  BYTES   INSTRUCTION
00    0B 0A   MOV B, #msg
02    12      LD A, [B]
03    51 09   JZ done
05    60      OUT A
06    44      INC B
07    50 02   JMP loop
09    00      HLT
0A    48 65 6C 6C 6F 2C 20 4B 52 38 21 0A 00  .asciz "Hello, KR8!\n"
```

- Code addresses come from the code map. Data runs are shown as `.byte` or
  `.asciz` when the bytes are printable and NUL-terminated.
- Branch/call targets and memory operands use symbols when an exact match
  exists: `JMP loop`, `LD A, [msg]`, `ST [buf], A`.
- Immediates print as `#0xNN`; the CLI may append `; 'A'` for printable bytes.
- `--raw` decodes linearly from `--base` to `--end` and has no symbols.
- The disassembler is a library first: the UI requests a vector of decoded
  instructions and data ranges, not text. The CLI formats that vector.

### 6.2 Library

```cpp
struct DecodedInstruction {
    std::uint8_t address;
    std::uint8_t size;
    std::array<std::uint8_t, 2> bytes;
    std::string text;
    int line;       // 0 if none
    bool valid;
};

struct DataRange {
    std::uint8_t address;
    std::uint16_t size;
    const Symbol* symbol; // optional
};

std::vector<DecodedInstruction> disassemble(const Image& image);
std::string instruction_help(std::string_view mnemonic);
```

`instruction_help` is generated from the same ISA table used by the CPU, the
assembler, and the UI tooltips.

---

## 7. VM and CLI

### 7.1 Commands

```text
kr8                                  open the UI (or print help if UI is not built)
kr8 ui [file]                        open the UI explicitly
kr8 as <source.asm> [-o out.k8] [--raw out.bin] [-l] [--no-source] [-q]
kr8 dis <image.k8|image.bin> [--raw] [--base A] [--end A] [--symbols] [--source] [--data]
kr8 run <image.k8|image.bin> [--raw] [--input FILE] [--max-instructions N]
                             [--trace] [--stats] [--entry A]
kr8 sym <image.k8>
kr8 version | --version
kr8 help [command]
```

### 7.2 `run`

- Load `.k8` (validate CRC and structure) or a 256-byte `.bin` (`--raw`). A raw
  file shorter than 256 bytes is zero-filled; a longer file is an error.
- Reset to the header entry, or `--entry`.
- Execute until `HLT`, `BRK`, a fault, the instruction budget, or end of input.
- `OUT` writes raw bytes to stdout. `IN` reads from `--input` or stdin; the CLI
  host may block to obtain a byte, and at EOF it returns no byte forever.
- `--trace` prints one line per instruction to stderr:

```text
pc=00 A=41 B=0A SP=FF F=Z---  MOV B, #0A
```

- `--stats` prints instructions, cycles, output bytes, and elapsed host time.
- `--max-instructions` defaults to 10,000,000. Exceeding it is exit code 3.
- Exit codes:
  - 0: `HLT`, `BRK`, or normal completion
  - 1: load/assemble error
  - 2: runtime fault (illegal opcode, stack fault, PC wrap)
  - 3: instruction budget exceeded
- `kr8 run` is deterministic: no wall clock or randomness inside the VM. Host
  elapsed time is reported only by `--stats`.

### 7.3 Machine API

```cpp
struct StepResult {
    enum class Status { Ok, Halted, Break, Fault, Budget };
    Status status;
    std::uint8_t pc;
    std::uint16_t cycles;
    std::string message; // fault text when applicable
};

class Cpu {
public:
    void reset(const Image& image);
    StepResult step(Memory& memory);
    // registers, flags, pc, sp, stack depth, halted, fault
};

class Machine {
public:
    void load(const Image& image);
    StepResult step();
    void set_out(std::function<void(std::uint8_t)>);
    void set_in(std::function<std::optional<std::uint8_t>()>);
    // console capture, stats
};
```

The CPU is pure and testable: it takes a 256-byte `Memory`, an out callback, and
an in callback; it has no filesystem, console, or clock dependencies.

---

## 8. Debug session

`Session` is the portable debug model. It owns the image, CPU, memory, symbols,
line map, breakpoints, shadow call stack, console, input queue, and stats. The
UI never touches the CPU directly; it renders a snapshot.

### 8.1 API sketch

```cpp
enum class StepMode { LineIn, LineOver, LineOut };

enum class RunStop { Budget, Halted, Breakpoint, Fault, Break };

struct Frame {
    int index;
    std::uint8_t pc;
    std::uint8_t return_address;
    int line;
    std::string function;
};

struct Snapshot {
    bool loaded;
    bool halted;
    bool running;
    std::uint8_t a, b, pc, sp, flags;
    std::uint64_t instructions;
    std::uint64_t cycles;
    int line;
    std::string function;
    std::string console;
    std::string message;
    std::vector<Frame> frames;
};

class Session {
public:
    bool load_image(const std::filesystem::path&, std::string& error);
    bool assemble(const std::filesystem::path&, std::string& error);
    bool restart(std::string& error);
    void unload();

    bool step_instruction();
    bool step_instruction_over();
    bool step_instruction_out();
    bool step_line(StepMode);
    RunStop run_slice(int max_instructions);
    void start_run();
    void stop_run(std::string reason);

    Snapshot snapshot() const;

    const std::vector<DecodedInstruction>& disassembly() const;
    const std::vector<std::string>& source_lines() const;
    const std::set<int>& executable_lines() const;
    const std::vector<Symbol>& symbols() const;
    int line_at(std::uint8_t pc) const;
    const Symbol* symbol_at(std::uint8_t address) const;

    void toggle_address_breakpoint(std::uint8_t);
    void toggle_line_breakpoint(int);
    void toggle_symbol_breakpoint(const std::string&);
    bool has_breakpoint(std::uint8_t) const;
    bool has_line_breakpoint(int) const;

    void set_register(int index, std::uint8_t value); // 0=A,1=B,2=PC,3=SP
    void set_flag(int bit, bool value);
    std::uint8_t read8(std::uint8_t address);
    void write8(std::uint8_t address, std::uint8_t value);

    void push_input(std::uint8_t byte);
    void clear_input();
    const std::string& input_queue() const;

    std::uint64_t instructions_executed() const;
    std::uint64_t cycles() const;
    std::uint64_t run_time_ms() const;
    void reset_stats();
};
```

### 8.2 Loading and building

- `load_image` reads and validates `.k8`. On failure, the previous image stays
  loaded and the error is reported.
- `assemble` reads the `.asm`, assembles in memory, writes the `.k8` next to the
  source (the same artifact the CLI would produce), then loads it.
- `restart` resets the CPU to the image entry, clears the shadow call stack and
  console output, and keeps breakpoints. Stats reset.
- Assembly failure keeps the old image and shows diagnostics; it does not destroy
  the debug session.
- Loading a `.k8` with embedded source shows that source. Loading one without
  source shows disassembly and symbols only.

### 8.3 Stepping

- `step_instruction`: execute exactly one instruction. `BRK` halts; Continue/step
  skips it as described in §3.9.
- `step_instruction_over`: if the current instruction is `CALL`, execute the
  call and then run until the shadow call depth returns to the starting depth
  (the call has returned); otherwise one instruction.
- `step_instruction_out`: run until the shadow call depth is below the starting
  depth. At top level this is a no-op.
- `step_line(LineIn)`: run until the source line changes, at any depth.
- `step_line(LineOver)`: run until the source line changes and the depth is at or
  below the starting depth.
- `step_line(LineOut)`: run until the source line changes and the depth is below
  the starting depth.
- Every step has a 1,000,000-instruction budget. Exhausting it returns a
  "step budget" stop instead of hanging.
- A breakpoint hit during a step stops the step with reason `Breakpoint`.
- If a line has no instructions (comment, label, data), it cannot be the current
  line; the source pane dims it and a breakpoint click snaps forward to the next
  executable line, reporting the move in the status bar.

### 8.4 Breakpoints

- **Address breakpoints**: exact `PC` matches from the disassembly gutter or a
  symbol.
- **Line breakpoints**: `(file, line)` predicates checked against `line_at(pc)`.
  Multiple addresses for one line all hit; no "first address only" bug.
- **Symbol breakpoints**: a code/data symbol resolves to an address breakpoint.
- Breakpoints are checked when execution arrives at a new `PC`, before executing
  that instruction. Continuing from a breakpoint skips the check once, so the
  instruction at the current `PC` runs.
- A step stops for a breakpoint before executing the breakpointed instruction.
- Breakpoints survive `restart` and a successful reassemble. Line breakpoints
  are remapped by line number; address breakpoints stay at their addresses and
  are clamped/dropped if the new image is shorter or the address is no longer
  code.

### 8.5 Shadow call stack

- `CALL`: push `{callee, return_address, caller_line}`.
- `RET`: pop.
- `RET` with an empty shadow stack: report "return without call" and pause. This
  catches a `JMP` into a subroutine or a hand-built return address.
- On `RET`, compare the popped `PC` with the shadow frame's recorded return
  address. A mismatch means the stack was unbalanced (`PUSH` without `POP`,
  wrong `RET`, or a hand-built return); report "return address mismatch" and
  pause. This is best-effort but catches the common stack bug.
- The current function is the nearest `Code` symbol at or below `PC`.
- The Call stack pane shows `#0 current`, `#1 caller`, ...; clicking a frame
  jumps to its recorded `PC`/return address and highlights the caller line.
- The UI labels the pane "Call stack (best effort)" and offers Restart if the
  program is doing something outside the structured model.

### 8.6 Console and input

- `OUT` appends to `console_`. The CLI writes raw bytes; the session keeps a
  string for the UI. The UI renders printable ASCII and escapes other bytes as
  `\xNN` (or a middle dot) so binary output is visible.
- `IN` consumes from `input_`; if empty, it returns no byte and sets `Z=1`.
- The UI's console input box queues bytes. Typing sends characters; Enter sends
  a newline; a "Send" button flushes the current line. `Clear` empties the
  queue. `push_input`/`clear_input` are also used by tests.
- No wait state is required. A polling loop stays responsive because the UI runs
  in slices. A future polish item may detect a long empty-`IN` poll run and show
  "waiting for input" without changing semantics.

### 8.7 Editing

- Registers and flags can be edited only while paused.
- `PC` edits move the current line/symbol immediately. `SP` edits invalidate the
  shadow call stack until Restart (the UI shows "call stack unavailable").
- Memory edits write live RAM; editing a code byte changes what executes. The
  UI marks edited bytes amber and warns in the status bar.
- All edits are validated (1–2 hex digits, `0..255`) and applied atomically.

### 8.8 Stats

- `instructions_executed_`: retired instructions.
- `cycles_`: from the CPU's simple model.
- `run_time_ms_`: host steady-clock time only while running; not part of the VM.
- `reset_stats` zeroes counters without changing machine state.
- `restart` resets counters and console output, but keeps breakpoints.

---

## 9. Windowed debugger

### 9.1 Reuse strategy

Port KR16's UI stack first, then replace the application layer:

```text
include/kr8/ui/{geometry,canvas,input,window}.hpp   copied, namespace renamed
src/platform/ui/win32/                              copied, namespace renamed
src/platform/ui/sdl/                                copied, namespace renamed
src/ui/text_field.{hpp,cpp}                         copied
src/resources/embedded_fonts.{hpp,cpp}              copied/regenerated
src/ui/theme.hpp                                    new, extracted palette
src/ui/help.{hpp,cpp}                               new, generated from ISA table
src/ui/app.{hpp,cpp}                                new, KR8 panes
src/ui/run_win32.cpp, run_sdl.cpp                   adapted
src/ui/main.cpp                                     smoke entry
cmake/CheckPlatformBoundary.cmake                   adapted
```

The platform interfaces stay identical apart from the namespace. That keeps the
Win32 and SDL backends proven and reduces the UI port to layout, painting, hit
testing, and session wiring.

### 9.2 Visual language

Use KR16's exact palette and typography. This is what makes the debugger feel
like the same family.

| Token | RGB |
|---|---|
| `kBg` | 22, 24, 29 |
| `kPanel` | 29, 31, 38 |
| `kPanelAlt` | 35, 38, 47 |
| `kBorder` | 52, 56, 68 |
| `kText` | 214, 218, 226 |
| `kTextDim` | 118, 124, 140 |
| `kAccent` | 82, 156, 255 |
| `kAccentDim` | 44, 74, 120 |
| `kHighlight` | 38, 64, 110 |
| `kStmt` | 31, 43, 58 |
| `kRed` | 230, 90, 90 |
| `kGreen` | 130, 200, 150 |
| `kAmber` | 235, 185, 95 |
| `kButton` | 52, 56, 68 |
| `kButtonHover` | 68, 74, 90 |
| `kButtonOff` | 38, 41, 50 |

Fonts: JetBrains Mono 15 for code and 13 for small mono; Inter 14 regular and
semibold for UI. Embed the subsets and the OFL licence files, exactly as KR16
does. The Win32 backend uses `AddFontMemResourceEx`; SDL uses stb_truetype with
a glyph atlas.

Layout constants from KR16 that transfer directly: padding 8, top bar 36, pane
header 30, status bar 24, gutter 90, source line height 20, scrollbar width 6.
KR8-specific values are noted below.

### 9.3 Layout

Reference size: 1280x820; minimum: 1024x700. The whole RAM fits on screen.

```text
+--------------------------------------------------------------------------+
| Load… | Assemble | Run | Break | Restart | Reset stats | INS 0  TIME ... |
+----------------------------------+---------------------------------------+
| Source  (line steps)   Over In Out| Disassembly  (instruction steps)     |
| 001  ; hello.asm                 | ADDR  BYTES  INSTRUCTION   Over In Out|
| 002  start:                      | 00    0B 0A  MOV B, #msg              |
|>003          MOV B, #msg         | 02    12     LD A, [B]                |
| 004  loop:                       | 03    51 09  JZ done                  |
| 005          LD A, [B]           | ...                                   |
| ...                              |---------------------------------------|
|                                  | Registers  A 00  B 00  PC 0000  SP FF |
|                                  | FLAGS: [Z] [C] [N] [V]                |
+----------------------------------+---------------------------------------+
| Memory  16 x 16                  | Symbols      | Call stack  | Console   |
| 00  00 00 ... 00  | ............. | start 0000   | #0 start    | Hello,    |
| 10  00 00 ... 00  | ............. | msg   000A   | line 3      | KR8!      |
| ...                              | ...          |             | [input]   |
+----------------------------------+---------------------------------------+
| pc=0x0002  line 3  instr 12  cycles 14  [paused]   F5 run  F6 break      |
+--------------------------------------------------------------------------+
```

Pane responsibilities:

- **Source** (left, top): embedded or opened assembly text, line numbers, current
  line highlight, breakpoint gutter, dimmed non-executable lines, Over/In/Out
  buttons, scrollbar, auto-follow PC. Header says "Source (line steps)".
- **Disassembly** (right, top): every decoded instruction with `ADDR`, `BYTES`,
  `INSTRUCTION`; PC row highlighted; breakpoint gutter; symbol-resolved operands;
  hover help for the mnemonic and operand; instruction-step buttons.
- **Registers/flags** (right, under disassembly): `A`, `B`, `PC`, `SP`; flags
  `Z C N V` as clickable boxes; hover shows hex/decimal/name; click-to-edit
  registers; changed values highlighted for one stop.
- **Memory** (left, bottom): the full 16x16 grid. Address column, 16 hex bytes
  per row, ASCII column. PC and SP bytes highlighted, current data symbol shaded,
  hover shows address/hex/decimal/symbol, click edits a byte. No scrolling is
  needed; the entire address space is visible.
- **Symbols** (bottom, middle-left): code/data/const groups; name, value, size or
  short ASCII preview; click to jump; double-click or gutter click toggles an
  address breakpoint; current symbol highlighted.
- **Call stack** (bottom, middle-right): shadow frames; click to jump; "best
  effort" label; empty state.
- **Console** (bottom, right): output text, input queue, input field, Send/Clear.
- **Status** (bottom): `pc`, line, instruction count, cycles, run state, message,
  and keyboard hints.

The exact split can be tuned during implementation, but the pane set and the
"source + disassembly + registers + full memory + symbols + stack + console"
structure are the target. The memory grid replaces KR16's two-row strip because
256 bytes fit on one screen and seeing the whole machine is the teaching point.

### 9.4 Top bar

Buttons: `Load…`, `Assemble`, `Run`, `Break`, `Restart`, `Reset stats`.
Counters: `INS`, `CYCLES`, `TIME`, `IPS`. Button states:

- `Run` disabled while running; `Break` enabled only while running.
- `Assemble` enabled when the current file is `.asm`; disabled for a loaded
  `.k8` (use `Load` or `Restart`).
- `Restart` enabled when an image is loaded.
- `Reset stats` always enabled when loaded; zeroes counters without changing
  state.

`Load…` opens `.asm`, `.k8`, or `.bin`. Opening a `.asm` assembles and loads it,
showing diagnostics on failure. Opening a `.k8` loads it. Opening a `.bin`
loads a raw 256-byte image at entry 0.

### 9.5 Source pane

- Debug mode is read-only. The **Edit** button enters an explicit Edit mode for
  `.asm`/`.s` files; `.k8`/`.bin` images stay read-only.
- In Edit mode the source is a multiline editor: caret, selection, clipboard,
  undo/redo, Home/End, Ctrl+arrows, PageUp/PageDown, auto-indent, syntax
  highlighting, and Ctrl+S save. Line numbers and breakpoint dots remain visible,
  but stepping is disabled until Assemble succeeds.
- **Assemble** compiles the editor buffer (not the stale file), writes the
  adjacent `.k8`, resets PC to entry, and returns to Debug mode. Assembly failure
  keeps Edit mode and shows diagnostics.
- **Ctrl+S** writes the `.asm`; unsaved changes prompt before Load or close.
- Line breakpoints are kept by line number and snapped forward to the nearest
  executable line after a successful assemble; address breakpoints are cleared.
- While editing, Disassembly and Symbols are marked `(stale)` and dimmed. The
  last image remains visible as context.
- Line numbers in a 3-digit gutter (sources are small; 2 digits are enough but 3
  leaves room for growth).
- Current line: `kHighlight` row with a 3px `kAccent` left bar.
- Breakpoint: 6x6 red square in the gutter; click toggles in Debug mode. Clicking
  a breakpoint on a non-executable line snaps forward to the next executable line
  and reports the move.
- Auto-scroll to the current line with a "follow" toggle.
- If the image has no embedded source, show "No source attached; use the
  disassembly." and disable line breakpoints/line steps and Edit mode.

### 9.6 Disassembly pane

- Every decoded instruction, with address, 1–2 bytes, and text. Because 256 bytes
  is small, the whole list can be built once per load; the UI just scrolls.
- Current PC: blue row highlight and a stronger PC marker.
- Current source line: shade all instructions belonging to that line. In v1 this
  is usually one instruction, but the mechanism is ready for macros/multi-
  instruction lines.
- Breakpoint gutter: click toggles an address breakpoint; right-click can offer
  "Run to here" via a temporary breakpoint (optional).
- Symbols: `JMP loop`, `CALL puts`, `LD A, [msg]`, `ST [buf], A`.
- Hover: mnemonic description and operand addressing help, generated from the
  ISA table.

### 9.7 Registers, flags, and memory

- Register strip: `A`, `B`, `PC`, `SP` in mono; hover shows name, hex, and
  unsigned/signed decimal. Click a cell to edit hex (`00`–`FF`); Enter applies,
  Esc cancels. `PC` edits update the current line; `SP` edits clear the shadow
  stack and show "call stack unavailable".
- Flags: `Z`, `C`, `N`, `V` boxes; set flags render green, clear flags dim; click
  toggles. Hover shows the flag name and state.
- Memory grid: 16 rows of 16 bytes. Address column `00`, `10`, ..., `F0`; 16 hex
  bytes; ASCII column. `PC` and `SP` are highlighted; the current data symbol is
  shaded; code bytes, operand bytes, and data bytes can use three subtle text
  tones. Hover a byte for address, hex, decimal, and symbol. Click to edit two
  hex digits. Editing a code byte is allowed but marked amber.
- Changed-value highlighting: after each stop, flash registers/flags (and
  optionally changed memory bytes) that differ from the previous snapshot. This
  is the single most useful teaching affordance from KR16's research note.

### 9.8 Symbols, call stack, console

- Symbols pane: grouped `Code`, `Data`, `Constants`; rows show `name`, `0xNN`,
  and size or a short printable preview for data. Click jumps the disassembly;
  double-click toggles a breakpoint; the symbol at `PC` is highlighted. A filter
  box is optional and deferred.
- Call stack: `#0 current line N`, `#1 caller line M`, ...; clicking a frame
  jumps to its recorded PC and highlights the caller line. The pane says "best
  effort" and explains the structured `CALL`/`RET` assumption in a tooltip.
- Console: scrollback output; input queue display; a single-line input field
  with `Send` and `Clear`; Enter sends the line plus `\n`; printable bytes render
  as text, others as `\xNN`. The console is the program's only I/O channel, so it
  is always visible.

### 9.9 Errors and help

- Assembly diagnostics appear in a scrollable Errors view (a bottom-pane tab or a
  modal). Each row is `file:line:col: message`; clicking jumps to the source
  line. Source gutter lines with errors are marked red. The previous image stays
  loaded.
- F1 opens a one-page help overlay: registers, flags, instruction table,
  directives, operand forms, and keyboard shortcuts. Because the ISA is 63
  instructions, the whole reference fits.
- Hover tooltips cover registers, flags, memory bytes, symbols, instructions, and
  addressing modes. Tooltip content comes from the shared ISA table so it cannot
  drift from the CPU.

### 9.10 Keyboard, settings, startup

| Key | Action |
|---|---|
| `F5` | Run |
| `F6` | Break |
| `F7` | Restart |
| `F8` | Assemble |
| `F10` | Step line |
| `F11` | Step instruction |
| `Ctrl+O` | Load |
| `Ctrl+R` | Assemble/reload |
| `F1` | Help |
| `Esc` | Close help/dialog/edit |

Settings (persisted to `%APPDATA%\KR8\settings.ini` on Windows,
`$XDG_CONFIG_HOME/kr8/settings.ini` or `~/.config/kr8/settings.ini` on Linux):
font size, run slice size, follow-PC, show-changed-values. Keep the settings
surface small in v1.

Startup: empty window with "Load a .asm or .k8 file, or drag one here." Opening
a path from the command line loads it. Drag/drop is a nice-to-have; KR16 has it
and the platform backends already support drop, so add it if it falls out
cheaply.

### 9.11 Windows and Linux entry points

- Windows: `kr8.exe` is a console-subsystem executable. Normal CLI commands run
  in the parent. No arguments spawns the same executable with `--ui` using
  `CreateProcessW` and `CREATE_NO_WINDOW`, then exits. `--ui [file]` is the
  internal child switch. A fatal UI error uses a message box and a log file.
- Linux: one ELF. No arguments runs the UI in-process; a terminal launch blocks
  until the window closes.
- If the UI is not built, no arguments prints usage and exits 2; `kr8 shell` is
  not needed because the CLI commands are explicit.
- `ui` is also an explicit command for both platforms.

---

## 10. Repository layout

```text
KR8/
  CMakeLists.txt
  CMakePresets.json
  README.md
  kr8-plan.md
  LICENSE
  THIRD_PARTY_NOTICES.md
  docs/
    isa.md
    assembler.md
    image-format.md
    debugger.md
    curriculum.md            (optional lesson sequence)
  include/kr8/
    isa.hpp
    image.hpp
    assembler.hpp
    disassembler.hpp
    cpu.hpp
    machine.hpp
    session.hpp
    version.hpp
    ui/{geometry,canvas,input,window}.hpp
  src/
    core/version.cpp
    isa/isa.cpp
    asm/assembler.cpp
    asm/expression.cpp
    dis/disassembler.cpp
    image/image.cpp
    vm/cpu.cpp
    vm/machine.cpp
    debug/session.cpp
    cli/main.cpp
    ui/app.cpp
    ui/app.hpp
    ui/text_field.cpp
    ui/text_field.hpp
    ui/theme.hpp
    ui/help.cpp
    ui/help.hpp
    ui/run_win32.cpp
    ui/run_sdl.cpp
    ui/main.cpp
    platform/console.cpp
    platform/ui/win32/{win32_canvas,win32_window,win32_launcher}.cpp/.hpp
    platform/ui/sdl/{sdl_canvas,sdl_window}.cpp/.hpp
    resources/embedded_fonts.cpp
    resources/embedded_fonts.hpp
  resources/fonts/
    Inter-Regular-Subset.ttf
    Inter-Bold-Subset.ttf
    JetBrainsMono-Regular-Subset.ttf
    JetBrainsMono-Bold-Subset.ttf
    OFL-Inter.txt
    OFL-JetBrainsMono.txt
    embed_fonts.py
  cmake/CheckPlatformBoundary.cmake
  examples/
    hello.asm
    count.asm
    echo.asm
    puts.asm
    memory.asm
    stack.asm
    flags.asm
    README.md
  tests/
    CMakeLists.txt
    harness.hpp
    test_isa.cpp
    test_cpu.cpp
    test_assembler.cpp
    test_image.cpp
    test_disassembler.cpp
    test_session.cpp
    test_cli.cpp
  third_party/
    stb/stb_truetype.h
    doctest/...
```

Dependency direction:

```text
CLI / UI ──► Session ──► Machine/Cpu ──► Image/Memory
                │
                └────► Assembler / Disassembler / Symbol/Line model

UI ──► Canvas/Window/Input interfaces ◄── Win32 or SDL backend
```

`src/ui` includes no OS headers. All Windows/SDL/X11/terminal code lives under
`src/platform`. A platform-boundary test enforces this.

---

## 11. Build system

- CMake 3.25+, C++20, no extensions.
- Targets:
  - `kr8_core` (static): ISA, image, assembler, disassembler, CPU, machine,
    session, version, CRC32, console host.
  - `kr8` (executable): CLI; links `kr8_core`; links `kr8_ui` when built.
  - `kr8_ui` (static): UI, platform backends, embedded fonts; links `kr8_core`.
  - `kr8_ui_smoke` (executable): UI smoke entry.
  - `kr8_tests` (executable): unit/integration tests.
- Options:
  - `KR8_BUILD_UI` (default ON on Windows/Unix)
  - `KR8_BUILD_TESTS` (default ON)
  - `KR8_SANITIZE` (default OFF; ASan + UBSan on GCC/Clang)
  - `KR8_SDL3_SOURCE_DIR` (optional offline SDL3 tree)
- Warnings:
  - MSVC: `/W4 /WX /permissive- /Zc:__cplusplus /Brepro`
  - GCC/Clang: `-Wall -Wextra -Wpedantic -Werror`
- Windows:
  - `/MT` static CRT.
  - UI links `user32 gdi32 comdlg32 shell32`.
  - No SDL, no DLLs; embedded fonts.
  - `/SUBSYSTEM:CONSOLE` for the one executable; the UI child has no console.
- Linux:
  - Static SDL3 via pinned `FetchContent` (same tag/commit as KR16); or
    `KR8_SDL3_SOURCE_DIR`.
  - `SDL_MAIN_HANDLED`, static SDL3, `dl pthread m`.
  - `third_party/stb/stb_truetype.h`.
- `KR8_BUILD_UI=OFF` must build and test the CLI headlessly.
- Presets: `windows-ninja`, `windows-vs2026`, `linux-gcc`.
- CI: Windows MSVC and Ubuntu GCC; UI ON and OFF; CTest; Xvfb UI smoke on
  Linux; platform-boundary test. Release workflow: Windows zip and Linux x64
  AppImage; ARM64 is optional/later.

---

## 12. Testing

### 12.1 Unit tests

**ISA table**

- Every defined opcode has a mnemonic, size, operand kind, and help text.
- No two opcodes collide; all undocumented opcodes are marked illegal.
- Assembler encoding and disassembler decoding agree for every opcode.
- Round-trip: assemble a snippet, disassemble it, reassemble the disassembly, and
  compare bytes (with canonical mnemonics).

**CPU**

- `ADD`/`ADC`/`SUB`/`SBC`/`CMP` flags for `0x00`, `0x7F`, `0x80`, `0xFF`, and
  carry/borrow edge cases.
- `AND`/`OR`/`XOR`/`TST`/`NOT` flags (`C=0`, `V=0`).
- `NEG`, `INC`, `DEC`, shifts, `CLC`/`SEC`.
- `MOV`/`LD`/`POP` set `Z`/`N`; `ST`/`PUSH` do not.
- Direct and `[B]` loads/stores at `0x00` and `0xFF`.
- `PUSH`/`POP`, `CALL`/`RET`, nested calls, stack overflow/underflow.
- Every branch: taken and not taken; signed `JLT/JGE/JGT/JLE` after `CMP`.
- `BRK` halt/resume; `HLT`; illegal opcode; 2-byte instruction at `0xFF` fault.
- `IN` with a byte, with no byte, and with a `0x00` byte (Z must be 0).
- Cycle counts for representative instructions.

**Assembler**

- Labels, forward references, code/data/const classification.
- `.org`, `.byte`, `.word`, `.ascii`, `.asciz`, `.space`, `.equ`, `.entry`.
- Expressions: precedence, unary, char literals/escapes, division by zero.
- Errors: duplicate label, undefined symbol, bad operand, `.org` backward,
  instruction crossing `0xFF`, image overflow, unterminated string, label with no
  code/data, stack headroom warning.
- Listing contains the expected addresses/bytes/symbols.

**Image**

- Write/read round-trip preserves RAM, entry, symbols, lines, sources, code map.
- CRC mismatch is rejected.
- Truncated/oversized counts and bad offsets are rejected safely.
- `.bin` export is exactly 256 bytes and matches `ram[]`.

**Disassembler**

- Code map honors data regions; no decode of data bytes.
- Symbol resolution for branches and memory operands.
- Raw mode with `--base`/`--end`.
- Every opcode decodes to a stable, canonical text.

**Session**

- Load, assemble, restart, unload.
- Instruction stepping; line stepping In/Over/Out on straight-line code, loops,
  calls, and recursion.
- Breakpoints: address, line, symbol; continue past a breakpoint; breakpoint hit
  during a step.
- Shadow call stack frames; `RET` without `CALL` fault.
- Input queue, `IN` polling, console output.
- Register/flag/memory edits; PC edit line update; SP edit stack invalidation.
- Stats and reset.

**CLI**

- `as` success/failure, output paths, listing, raw export, `--no-source`.
- `dis` output, symbols, raw mode.
- `run` output, trace, stats, input file, EOF, budget, exit codes.
- `sym`, `version`, `help`.

### 12.2 Golden programs

| Example | Teaches | Expected |
|---|---|---|
| `hello.asm` | `.asciz`, `[B]`, `OUT`, `JZ` on a load | `Hello, KR8!\n` |
| `count.asm` | `INC`, `DEC`, `JNZ`, character immediates | `0123456789\n` |
| `echo.asm` | `IN` polling, `CMP`, newline handling | echoes input up to newline |
| `puts.asm` | `CALL`/`RET`, subroutine, pointer argument | `KR8\n` |
| `memory.asm` | `ST [B]`, pointer walking, `CMP` | memory at `buf` = `01..08` |
| `stack.asm` | `PUSH`/`POP`, preserving `A` | `!A` |
| `flags.asm` | `CMP`, signed/unsigned branches | branch-selected characters |

Each example is assembled in a test, run headlessly, and compared byte-for-byte.
The memory example additionally inspects `buf` after the run. Examples are kept
small enough to leave generous stack headroom; the assembler's summary proves it.

### 12.3 UI tests

- A fake `Canvas` with fixed metrics tests layout rectangles, gutter hit testing,
  pane bounds, and button enablement without a window.
- Win32 and SDL smoke tests: load `hello.k8`, step, run to halt, Break, Restart,
  edit a register, set a line breakpoint, set an address breakpoint, check the
  memory grid and console.
- Screenshots for the README and docs: initial load, stepping, a breakpoint,
  registers/flags, memory, symbols, console, help.
- The smoke path must not require a real display on CI: SDL offscreen/Xvfb, Win32
  optional on a desktop runner.

### 12.4 CI gates

- Windows and Linux builds with UI ON and OFF.
- `ctest` green, including the platform-boundary check.
- ASan/UBSan on Linux.
- No warnings under `/W4 /WX` or `-Wall -Wextra -Wpedantic -Werror`.
- Release artifacts: single Windows exe with no DLL dependencies; Linux x64
  AppImage with static SDL3 and embedded fonts.

---

## 13. Milestones

Estimates are for one developer; the UI port is the largest block. "Part-time"
means roughly half-time.

`kr8-impl.md` turns these milestones into an ordered, task-level checklist
with per-task acceptance tests. This plan remains the design contract; the
implementation document is the working checklist.

### M0 — Skeleton and machine (2–3 days)

- Repo layout, CMake, presets, README, LICENSE, plan/docs skeleton.
- `isa.hpp`/`isa.cpp` single opcode table and mnemonic/help text.
- `Cpu`, `Memory`, `Machine`, `StepResult`, flags, stack depth/floor, faults.
- CPU unit tests for every instruction and flag edge case.

**Acceptance:** `kr8_core` builds; CPU tests pass; no UI yet.

### M1 — Image, assembler, disassembler (4–5 days)

- `.k8` writer/reader with CRC, symbols, lines, source, code map.
- Two-pass assembler: lexer, parser, expressions, directives, diagnostics,
  listing, summary.
- Disassembler library and CLI.
- `kr8 as`, `kr8 dis`, `kr8 sym`.
- Assembler/image/disassembler tests; all examples assemble.

**Acceptance:** examples produce correct images/symbols/lines; round-trip tests
pass; errors are actionable.

### M2 — VM and CLI (2–3 days)

- `kr8 run`: load, reset, step loop, console I/O, stdin/`--input`, trace, stats,
  budget, exit codes.
- Stack/fault diagnostics and PC-wrap handling.
- Golden example output tests.

**Acceptance:** all examples run with expected output; faults are precise.

### M3 — Debug session (4–5 days)

- `Session`: load/assemble/restart, snapshots, disassembly/source/symbol views.
- Breakpoints (address/line/symbol), stepping (instruction/line/over/out), budget.
- Shadow call stack and frames; console/input; stats; editing.
- Session unit tests and stepping transcripts.

**Acceptance:** line/instruction stepping is exact on loops, calls, recursion;
breakpoints survive restart; tests pass.

### M4 — UI foundation and Win32 (8–10 days)

- Port UI interfaces, `TextField`, theme, Win32 backend, embedded fonts.
- KR8 `App`: top bar, source, disassembly, registers/flags, full memory grid,
  symbols, call stack, console, status.
- Load/assemble/run/break/restart/reset stats, errors, editing, hover help.
- Keyboard shortcuts, help overlay, auto-follow, changed-value highlights.
- Layout/hit tests and Win32 smoke test; screenshots.

**Acceptance:** a student can load `hello.asm`, assemble, step both panes, set
breakpoints, inspect registers/memory/symbols, run, and see console output. The
window matches KR16's visual language.

### M5 — SDL3/Linux (4–6 days)

- Port SDL3 canvas/window, `stb_truetype` atlas, embedded fonts, file dialog with
  fallback, timers, DPI.
- Xvfb smoke test and Linux screenshots.
- Remove any Windows-only assumptions from `src/ui`.

**Acceptance:** the same feature checklist on Linux x64; no OS headers in
`src/ui`; CI green.

### M6 — Polish, docs, packaging (3–4 days)

- Help overlay completeness, tooltips, settings, drag/drop if cheap.
- README, ISA/assembler/image/debugger docs, curriculum, examples README.
- Windows single-exe dependency check; Linux AppImage; third-party notices.
- CI release workflow; final screenshots.

**Acceptance:** release artifacts run on clean machines; docs match behavior; all
tests green.

### Later (not required for v1)

- Local labels, `.include`, macros, multi-file programs.
- Conditional breakpoints, watch expressions, run-to-cursor.
- ARM64 Linux/AppImage.
- A "lesson mode" that highlights the datapath for the current instruction.
- Optional cycle-accurate model.

Rough total: 27–36 developer-days, or about 6–8 weeks full-time and 10–14 weeks
part-time. The UI port is the largest single block; the core machine, assembler,
image, and session are comparatively small.

---

## 14. Risks and mitigations

| Risk | Why it matters | Mitigation |
|---|---|---|
| 256 bytes is extremely tight | Programs, data, and stack collide; examples may not fit | Assembler budget summary and headroom warning; curated small examples; stack-floor fault with a precise message |
| Variable-length decode bugs | CPU, assembler, and disassembler disagree; stepping breaks | One shared ISA table; encode/decode round-trip tests; fuzz opcode streams |
| Non-blocking `IN` polling | A poll loop can burn instructions and look like a hang | Run in slices; console queue visible; document the poll pattern; optional "waiting" heuristic later |
| Shadow call stack drift | `JMP`-into-subroutine or hand-built returns confuse frames | Best-effort label; `RET` without `CALL` diagnostic; raw stack view; Restart |
| UI port is large | It can dominate the schedule | Port proven KR16 interfaces/backends first; new `App` only; Windows then SDL in separate milestones |
| SDL3/font packaging | Linux build fragility; Windows DLL leaks | Reuse KR16's pinned static SDL3 and embedded-font setup; dependency checks in CI |
| Absolute addresses only | No relocation, no position independence | Intentional: no linker and no separate load address; document it as a feature |
| 8-bit PC wrap | A 2-byte instruction at `0xFF` corrupts fetch | Assembler rejects; CPU faults; tests at the boundary |
| Embedded source/path issues | Windows paths with `\r`, `\n`, `\t` broke KR16's debugger | Store source as length-delimited UTF-8 in the blob, never escape-process paths; test a `C:\...` path |
| No editor in v1 | Students need two programs | Resolved by the post-v1 explicit Edit mode: edit, Ctrl+S, F8 assemble, then step |
| Line stepping is less central without C | Source lines mostly equal instructions | Keep line stepping for directives/data/future multi-instruction lines; emphasize instruction stepping |
| Cycle model is approximate | Students may over-interpret it | Label it as a simple estimate; document the formula; don't use it for correctness |

---

## 15. Settled and deliberately deferred decisions

Settled in the 2026-11-03 review:

- **Host stack** — C++20 + CMake; KR16 Win32 GDI and SDL3 backends; static SDL3
  on Linux; `/MT` on Windows.
- **ISA scope** — all 63 instructions stay, including `TST`, `ADC`/`SBC`,
  `JV`/`JNV`, and `JLT`/`JGE`/`JGT`/`JLE`.
- **`IN` at EOF** — no distinction from "no byte yet"; `Z=1` in both cases.
- **Cycles** — retain the simple, documented estimate; label it approximate.
- **Next document** — `kr8-impl.md`, the task-level work breakdown.

Deliberately deferred (not v1):

1. **Built-in editor scope** — implemented as the post-v1 Edit mode: selection,
   undo/redo, save/dirty state, syntax highlighting, and explicit assemble/reload.
2. **Local labels** — `@loop`/`.loop` scoped to the last global label. Useful,
   but not required for the first examples. The symbol table can add a scope
   field without an image-format break.
3. **`.include` and multi-file source** — the line table already has a file
   index; the assembler and UI need a file list, include paths, and per-file
   diagnostics.
4. **Macros** — useful for `puts`/`print` conveniences, but they complicate line
   mapping and the two-pass model. Defer.
5. **`BRK` semantics** — v1: halt on `BRK`, Continue skips it. If software
   breakpoints are added later, this may become a trap instruction.
6. **Waiting-for-input UI** — currently no special state. A future heuristic may
   pause after N empty `IN` polls and show "waiting for input"; the CPU semantics
   stay non-blocking.
7. **Linux packaging** — x64 AppImage in v1; ARM64 later.
8. **Settings surface** — font size, slice size, follow, changed values. More
   settings only if they earn their place.
9. **Raw stack view** — the Call stack pane is shadow-based. A toggle to show
   the raw bytes from `SP` to `0xFF` would help when the shadow stack is
   inconsistent.

---

## Appendix A — Example programs

### A.1 `hello.asm`

```asm
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
```

### A.2 `count.asm`

```asm
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
```

### A.3 `echo.asm`

```asm
; echo.asm — echo characters until Enter (0x0A).
loop:
        IN A                 ; Z=1 if no input is available
        JZ loop
        CMP A, #10
        JZ done
        OUT A
        JMP loop
done:
        HLT
```

### A.4 `puts.asm`

```asm
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
```

### A.5 `memory.asm`

```asm
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
```

### A.6 `stack.asm`

```asm
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
```

### A.7 `flags.asm`

```asm
; flags.asm — show signed comparison branches.
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
```

Expected output: `G`.

---

## Appendix B — UI feature checklist

| Feature | v1 target |
|---|---|
| Load `.asm`, `.k8`, `.bin` | Yes |
| Assemble current `.asm`; write `.k8` | Yes |
| Restart, Reset stats | Yes |
| Run in slices, Break | Yes |
| Source pane: line numbers, current line, gutter breakpoints, dim non-executable, auto-follow | Yes |
| Source line step Over/In/Out | Yes |
| Disassembly: address, bytes, instruction, PC highlight, gutter breakpoints, symbols | Yes |
| Instruction step Over/In/Out | Yes |
| Registers A/B/PC/SP; click-to-edit; hover decimal | Yes |
| Flags Z/C/N/V; click-to-toggle; changed highlight | Yes |
| Full 256-byte memory grid; PC/SP/symbol highlights; byte hover/edit | Yes |
| Symbols pane: code/data/const; navigate; breakpoint | Yes |
| Call stack (shadow, best effort) | Yes |
| Console output + input queue | Yes |
| Status bar: pc, line, instructions, cycles, state | Yes |
| Assembly diagnostics with source jump | Yes |
| F1 instruction/directive help; hover tooltips | Yes |
| Keyboard shortcuts | Yes |
| Settings (font, slice, follow) | Yes, minimal |
| Drag/drop | Nice-to-have |
| Built-in source editor | Later |
| Conditional breakpoints/watch | Later |
| Windows and Linux parity | Yes |

---

## Appendix C — Test matrix (summary)

| Area | Unit | Integration | UI |
|---|---|---|---|
| ISA table | Every opcode, size, help | Encode/decode round-trip | Hover help |
| CPU | Flags, memory, stack, branches, faults | Golden programs | Registers/flags |
| Assembler | Syntax, directives, expressions, errors | Examples, listing, summary | Error list/jump |
| Image | Round-trip, CRC, bounds | Load/disassemble | Symbols/source |
| Disassembler | Code map, symbols, raw | CLI output | Disassembly pane |
| Session | Steps, breakpoints, frames, input | Transcripts | Step buttons |
| CLI | Commands, exit codes | `as`→`run` pipeline | n/a |
| UI | Layout/hit tests | Smoke, screenshots | Manual checklist |

---

## Appendix D — Glossary

| Term | Meaning |
|---|---|
| `.k8` | Self-contained KR8 image: 256-byte RAM plus symbols, lines, source, code map, CRC |
| `.bin` | Raw 256-byte RAM image, no metadata |
| Code map | 256-bit map marking instruction start bytes |
| `image_high` | First byte after the highest emitted byte; the stack floor |
| Shadow call stack | Debugger-side `CALL`/`RET` frame stack, used for stepping and frames |
| Snapshot | One immutable view of machine state rendered by the UI |
| Line row | `(address, file, line)` mapping used for source stepping and breakpoints |
| Absolute target | An 8-bit branch/call address; no displacement or relocation |
