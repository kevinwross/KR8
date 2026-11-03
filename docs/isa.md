# KR8 ISA Reference

Status: normative for the M0.2 table  
Applies to: KR8 ISA v1, image format 1.0  
Companion design: [../kr8-plan.md](../kr8-plan.md)

This document is the normative instruction-set reference. The shared table in
`include/kr8/isa.hpp` / `src/isa/isa.cpp` is the implementation of this
document; if they disagree, this document wins and the table must be fixed.
Changes to the ISA update this document, the table, and the tests in the same
change.

## 1. Overview

KR8 is an 8-bit, byte-addressable, von Neumann teaching CPU with a 256-byte
address space.

- Data width: 8 bits.
- Address width: 8 bits.
- RAM: 256 bytes, shared by code, data, and stack.
- Instructions: 1 or 2 bytes; the opcode determines the length.
- No alignment requirement.
- No ROM, MMIO, interrupts, caches, pipelines, or delay slots.
- Console I/O is `OUT A` / `IN A`.

## 2. Registers

| Register | Width | Role |
|---|---|---|
| `A` | 8 | Accumulator; destination of every ALU operation |
| `B` | 8 | Index and counter; pointer for `[B]` addressing |
| `PC` | 8 | Program counter |
| `SP` | 8 | Stack pointer; points at the next free byte |
| `FLAGS` | 8 | `Z`, `C`, `N`, `V` in bits 0–3; bits 7–4 read as zero |

`A` is the only ALU destination. `B` supports `MOV`, `LD`/`ST`, `INC`, `DEC`,
`PUSH`, and `POP`, but not `ADD B, ...` or `CMP B, ...`. Move a value through
`A` for other operations.

## 3. Flags

| Bit | Name | Meaning |
|---|---|---|
| 0 | `Z` | Zero: the result or loaded value is `0x00` |
| 1 | `C` | Carry / no-borrow |
| 2 | `N` | Negative: bit 7 of the result or loaded value |
| 3 | `V` | Signed overflow |
| 7–4 | — | Reserved, read as zero |

`C` is carry-out for addition and "no borrow" for subtraction/comparison.

`V` is signed overflow:

- Addition: both operands have the same sign and the result has the opposite
  sign.
- Subtraction/comparison: the operands have different signs and the result sign
  differs from the minuend.

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

`MOV`, `LD`, and `POP` set `Z`/`N` so `LD A, [B]` followed by `JZ` is a natural
string loop. `ST` and `PUSH` do not change flags.

## 4. Memory map and stack

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

- RAM is 256 bytes, read/write, unified.
- `image_high` is the high-water mark of emitted bytes. It is the stack floor.
- `SP` starts at `0xFF` and points at the next free byte:
  - `PUSH`: `mem[SP] = value; SP--`.
  - `POP`: `SP++; value = mem[SP]`.
- The CPU tracks stack depth in addition to `SP`, so overflow and underflow are
  detected even when `SP` wraps.
- A push below `image_high`, or a 257th push, is a stack-overflow fault.
- A pop or `RET` at depth 0 is a stack-underflow fault.
- No alignment is required. A 2-byte instruction must not start at `0xFF`.

Reset state:

| State | Value |
|---|---|
| `PC` | Image header `entry` (default: first instruction) |
| `SP` | `0xFF` |
| `A`, `B` | `0x00` |
| `FLAGS` | `0x00` |
| RAM | The 256-byte image; unwritten bytes are zero |
| Stack depth | 0 |
| Stack floor | `image_high` |

`0x00` is `HLT`, so running into uninitialized memory stops cleanly.

## 5. Instruction encoding

```text
1-byte instruction:   [opcode]
2-byte instruction:   [opcode] [operand]

operand kinds:
  n  = 8-bit immediate
  a  = 8-bit absolute address
  t  = 8-bit absolute branch/call target
```

- The opcode determines whether an operand byte follows.
- `PC` advances by 1 or 2.
- A 2-byte instruction at `0xFE` is legal: it occupies `0xFE` and `0xFF`, and
  the next `PC` is `0x00`.
- A 2-byte instruction at `0xFF` is illegal: its operand would be fetched from
  `0x100`, which wraps to `0x00`. The assembler rejects it; the CPU faults.
- A 1-byte instruction at `0xFF` is legal and wraps `PC` to `0x00`.
- `CALL` pushes the address after the call (one byte). A call at `0xFE` pushes
  `0x00`.
- `RET` pops one byte into `PC`.

## 6. One-byte instructions

`A`/`B` are registers; `[B]` is memory at `B`. `Flags` lists the flags whose
value is defined after the instruction.

| Opcode | Mnemonic | Flags | Operation |
|---|---|---|---|
| `0x00` | `HLT` | — | Stop the CPU normally |
| `0x01` | `NOP` | — | No operation |
| `0x02` | `BRK` | — | Halt with a breakpoint stop |
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
| `0x25` | `AND A, B` | Z C N V | `A <- A & B`; `C=0`, `V=0` |
| `0x26` | `OR A, B` | Z C N V | `A <- A \| B`; `C=0`, `V=0` |
| `0x27` | `XOR A, B` | Z C N V | `A <- A ^ B`; `C=0`, `V=0` |
| `0x28` | `TST A, B` | Z C N V | flags of `A & B`; no write; `C=0`, `V=0` |
| `0x40` | `NOT A` | Z C N V | `A <- ~A`; `C=0`, `V=0` |
| `0x41` | `NEG A` | Z C N V | `A <- 0 - A` |
| `0x42` | `INC A` | Z C N V | `A <- A + 1` |
| `0x43` | `DEC A` | Z C N V | `A <- A - 1` |
| `0x44` | `INC B` | Z C N V | `B <- B + 1` |
| `0x45` | `DEC B` | Z C N V | `B <- B - 1` |
| `0x46` | `SHL A` | Z C N V | `C <- A[7]; A <- A << 1`; `V=0` |
| `0x47` | `SHR A` | Z C N V | `C <- A[0]; A <- A >> 1`; `V=0` |
| `0x48` | `SAR A` | Z C N V | `C <- A[0]; A <- arithmetic A >> 1`; `V=0` |
| `0x49` | `CLC` | C | `C <- 0` |
| `0x4A` | `SEC` | C | `C <- 1` |
| `0x60` | `OUT A` | — | Send `A` to the console |
| `0x61` | `IN A` | Z | If a byte is available: `A <- byte`, `Z <- 0`; else `A <- 0`, `Z <- 1` |

## 7. Two-byte instructions

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
| `0x35` | `AND A, #n` | n | Z C N V | `A <- A & n`; `C=0`, `V=0` |
| `0x36` | `OR A, #n` | n | Z C N V | `A <- A \| n`; `C=0`, `V=0` |
| `0x37` | `XOR A, #n` | n | Z C N V | `A <- A ^ n`; `C=0`, `V=0` |
| `0x38` | `TST A, #n` | n | Z C N V | flags of `A & n`; no write; `C=0`, `V=0` |
| `0x50` | `JMP t` | t | — | `PC <- t` |
| `0x51` | `JZ t` | t | — | if `Z=1`, `PC <- t` |
| `0x52` | `JNZ t` | t | — | if `Z=0`, `PC <- t` |
| `0x53` | `JC t` | t | — | if `C=1`, `PC <- t` |
| `0x54` | `JNC t` | t | — | if `C=0`, `PC <- t` |
| `0x55` | `JN t` | t | — | if `N=1`, `PC <- t` |
| `0x56` | `JNN t` | t | — | if `N=0`, `PC <- t` |
| `0x57` | `JV t` | t | — | if `V=1`, `PC <- t` |
| `0x58` | `JNV t` | t | — | if `V=0`, `PC <- t` |
| `0x59` | `JLT t` | t | — | if `N != V`, `PC <- t` |
| `0x5A` | `JGE t` | t | — | if `N == V`, `PC <- t` |
| `0x5B` | `JGT t` | t | — | if `Z=0 and N == V`, `PC <- t` |
| `0x5C` | `JLE t` | t | — | if `Z=1 or N != V`, `PC <- t` |
| `0x5D` | `CALL t` | t | — | push `PC_next`; `PC <- t` |

## 8. Branch aliases

The assembler accepts KR16-style aliases. The disassembler prints the canonical
`Jcc` names.

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

## 9. Console I/O

`OUT A`:

- Sends one raw byte to the host console.
- CLI: written to stdout.
- UI: appended to the Console pane.
- No flags change.

`IN A`:

- Asks the host for one byte without blocking the CPU.
- Byte available: `A = byte`, `Z = 0`. A received `0x00` is a real byte, so `Z`
  is still 0.
- No byte: `A = 0`, `Z = 1`. This is the polling condition.
- `N`, `C`, `V` are unchanged.
- At EOF, `IN` behaves like "no byte yet": `Z=1` forever. EOF is not
  distinguished from no input in v1.
- CLI host: may block on stdin to obtain a byte; at EOF it returns no byte.
- UI host: returns a byte from the console input queue or no byte immediately.

Polling pattern:

```asm
wait:
    IN A
    JZ wait
```

## 10. Faults

| Fault | Cause | PC | Recovery |
|---|---|---|---|
| `IllegalOpcode` | Opcode not in the table | Faulting opcode | Restart |
| `StackOverflow` | Depth 256, or push below `image_high` | Faulting instruction | Restart |
| `StackUnderflow` | Pop/`RET` at depth 0 | Faulting instruction | Restart |
| `PcWrap` | 2-byte instruction at `0xFF` | `0xFF` | Restart |
| `Break` | `BRK` | The `BRK` | Continue/step skips it |
| `Halt` | `HLT` | The instruction after `HLT` | Restart |

`BRK` is a teaching/debugging stop. The CPU halts with `Break` and leaves `PC`
on the `BRK`; the session advances `PC` and clears the halt on Continue/Step.

Reserved opcodes: `0x0C–0x0F`, `0x16–0x1F`, `0x29–0x2F`, `0x39–0x3F`,
`0x4B–0x4F`, `0x5E–0x5F`, `0x62–0xFF`. Executing one raises
`IllegalOpcode`.

## 11. Cycle model

The cycle count is a simple teaching estimate, not hardware timing:

```text
cycles = bytes fetched
       + 1 for each memory access (LD, ST, PUSH, POP, CALL's push, RET's pop)
       + 1 for each taken JMP/Jcc/CALL
```

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

## 12. Encoding examples

```text
MOV A, #0x41     0A 41
MOV B, #0x0A     0B 0A
LD A, [0x20]     10 20
LD A, [B]        12
ST [0x20], A     13 20
ST [B], A        15
ADD A, B         20
ADD A, #1        30 01
CMP A, #10       34 0A
JZ 0x0A          51 0A
CALL 0x10        5D 10
OUT A            60
HLT              00
```

`0x00` is `HLT`, which is why blank or uninitialized memory stops cleanly.
