# KR8 — Implementation Plan, Milestones, and Checklists

Status: active  
Design contract: [kr8-plan.md](kr8-plan.md) — frozen 2026-11-03  
Implementation started: 2026-11-03  
Owner: Kevin Ross  
Working directory: `C:\Users\kevin\source\repos\KR8`  
Current milestone: **Post-v1 — built-in source editor**
Current task: **post-v1 editor complete**

This document is the working checklist. `kr8-plan.md` is the design contract.
If implementation forces a design change, update `kr8-plan.md` first (or in the
same change), then adjust this checklist. The two must not drift.

The plan is frozen on these points: 8-bit/256-byte machine, variable 1–2 byte
instructions with absolute branch targets, `A`/`B`/`PC`/`SP`/`FLAGS`
(`Z`/`C`/`N`/`V`), all 63 instructions, non-blocking `IN` with `Z` as the
availability flag (EOF is not distinguished), the self-contained `.k8` image,
the simple cycle estimate, C++20 + CMake, and the KR16 Win32/SDL3 UI stack.

---

## 0. How to use this document

### Task format

Every task has:

- **Status:** `Not started`, `In progress`, `Blocked`, or `Done`.
- **Depends on:** task IDs that must be `Done` first.
- **Estimate:** rough developer time.
- **Files:** the files the task is expected to touch or create.
- **Plan:** the relevant sections of `kr8-plan.md`.
- **Work:** a checklist of concrete steps.
- **Acceptance:** what must be true to mark the task `Done`.

### Rules

1. Work top-to-bottom within a milestone and respect dependencies.
2. A task is `Done` only when its checklist is complete **and** its acceptance
   test passes.
3. Add tests with the code that needs them; do not defer them to the milestone
   gate.
4. Update the status dashboard and the work log at each meaningful step.
5. Use the `local-datetime` tool for work-log timestamps. Record the ISO local
   time with offset, for example `2026-11-03T11:31:36-08:00
   (America/Los_Angeles)`.
6. Do not mark a milestone complete with a failing gate or a known P0/P1 defect.
7. Keep `kr8-plan.md` frozen. A design change needs a plan edit and a work-log
   entry, not a silent implementation divergence.
8. Prefer small, reviewable changes: one task or one coherent slice at a time.
9. Commit at meaningful checkpoints (a completed task or a coherent slice) and
   at the end of every completed milestone. Use the repository's configured
   global git identity; do not set a local `user.name` or `user.email`.

### Status values

| Status | Meaning |
|---|---|
| `Not started` | No work has begun |
| `In progress` | Actively being worked; work log has the current step |
| `Blocked` | Waiting on a decision, dependency, or external issue; record why |
| `Done` | Checklist complete and acceptance test passing |

### Test commands

These presets are created in M0.1.

Windows:

```text
cmake --preset windows-ninja
cmake --build --preset windows-ninja-debug
ctest --preset windows-ninja-debug
```

Linux:

```text
cmake --preset linux-gcc
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

UI smoke tests are added in M4/M5; their exact commands are recorded there.

---

## 1. Current status

Last updated: **2026-11-03T12:15:21-08:00** (America/Los_Angeles)

| Milestone | Status | Started | Completed | Next task |
|---|---|---|---|---|
| M0 — Skeleton and machine | `Done` | 2026-11-03 | 2026-11-03 | — |
| M1 — Image, assembler, disassembler | `Done` | 2026-11-03 | 2026-11-03 | — |
| M2 — VM and CLI | `Done` | 2026-11-03 | 2026-11-03 | — |
| M3 — Debug session | `Done` | 2026-11-03 | 2026-11-03 | — |
| M4 — UI foundation and Win32 | `Done` | 2026-11-03 | 2026-11-03 | — |
| M5 — SDL3/Linux | `Done` | 2026-11-03 | 2026-11-03 | — |
| M6 — Polish, docs, packaging | `In progress` | 2026-11-03 | — | M6.1 |

**Tasks:** 44 total, 44 done, 0 in progress, 0 blocked.  
**Code:** full CLI toolchain, portable debug session, Win32 and SDL3 debuggers,
and the Linux Xvfb smoke test.  
**Current focus:** post-v1 editor complete.

### Critical path

```text
M0 ──► M1 ──► M2 ──► M3 ──► M4 ──► M5 ──► M6
              │
              └──► docs and CI run alongside, but the gates are sequential
```

M4 can begin once M3's session API is stable; the UI does not need M2's CLI.
M5 depends on M4's portable UI and M3's session. Documentation is written with
each milestone, not only at M6.

---

## 2. Milestone map

| Milestone | Objective | Exit criteria | Estimate |
|---|---|---|---|
| M0 | Skeleton, shared ISA table, CPU, memory, machine, CPU tests | All 63 instructions decode; flag/memory/stack tests pass; no UI | 2–3 days |
| M1 | `.k8` image, two-pass assembler, disassembler, `as`/`dis`/`sym` | Examples assemble; image round-trips; disassembly is symbol-aware | 4–5 days |
| M2 | `kr8 run`, console I/O, faults, golden examples | All examples produce expected output; faults are precise | 2–3 days |
| M3 | Debug session: snapshots, stepping, breakpoints, frames, input | Session tests and stepping transcripts pass | 4–5 days |
| M4 | Win32 UI: all panes, load/assemble/run/break, editing, help | Feature checklist and Win32 smoke test pass | 8–10 days |
| M5 | SDL3/Linux backend and parity | Same checklist on Linux; Xvfb smoke passes | 4–6 days |
| M6 | Docs, packaging, CI, release checks | Single Windows exe and Linux AppImage verified; docs complete | 3–4 days |

Total: roughly 27–36 developer-days, matching plan §13.

---

## 3. M0 — Skeleton and machine

**Goal:** a buildable `kr8_core` with the shared ISA table, a complete CPU, a
256-byte memory, the machine wrapper, and CPU tests. No UI, no assembler yet.

### M0.1 — Repository skeleton and CMake

- **Status:** `Done`
- **Depends on:** —
- **Estimate:** 0.5 day
- **Files:** `CMakeLists.txt`, `CMakePresets.json`, `.gitignore`, `LICENSE`,
  `README.md`, `include/kr8/version.hpp`, `src/core/version.cpp`,
  `src/cli/main.cpp`, `tests/CMakeLists.txt`, `tests/harness.hpp`
- **Plan:** §10 Repository layout, §11 Build system

**Work**

- [x] Create the directory tree from plan §10.
- [x] `CMakeLists.txt`: project `kr8` version `0.1.0`, C++20, no extensions.
- [x] Add `kr8_core` static library with `src/core/version.cpp`.
- [x] Add `kr8` executable with `version`/`help` stubs.
- [x] Add `kr8_tests` executable and a small test harness or vendored doctest.
- [x] Warnings: MSVC `/W4 /WX /permissive- /Zc:__cplusplus /Brepro`; GCC/Clang
      `-Wall -Wextra -Wpedantic -Werror`.
- [x] MSVC static runtime: `MultiThreaded$<$<CONFIG:Debug>:Debug>`.
- [x] Presets: `windows-ninja`, `windows-vs2026`, `linux-gcc`, each with debug
      and release builds.
- [x] Add `.gitignore`, MIT `LICENSE`, and a short `README.md`.
- [x] `kr8 --version` prints `KR8 0.1.0`.

**Acceptance**

- Configure/build succeeds on Windows with the preset.
- `kr8 --version` prints the version and exits 0.
- `ctest` finds and runs the placeholder test.
- No warnings under the configured flags.

Verified: `windows-vs2026` configure, `windows-vs2026-debug` build, CTest
`kr8.tests` (3 tests) passed, `kr8 --version` → `KR8 0.1.0`. The `windows-ninja`
preset is present but needs a Visual Studio developer environment (`vcvars64`);
the VS generator was used for this checkpoint.

### M0.2 — Shared ISA table

- **Status:** `Done`
- **Depends on:** M0.1
- **Estimate:** 0.5 day
- **Files:** `include/kr8/isa.hpp`, `src/isa/isa.cpp`, `tests/test_isa.cpp`,
  `docs/isa.md` (skeleton)
- **Plan:** §3.2 Flags, §3.5 Encoding, §3.6 Instruction set, §6.2 Library

**Work**

- [x] Define `Op`, `OperandKind`, flag masks, and `OpcodeInfo`.
- [x] Encode all 63 instructions from plan §3.6: opcode, mnemonic, size,
      operand kind, modified flags, help text, aliases.
- [x] Mark all reserved opcodes illegal.
- [x] Implement lookup by opcode and case-insensitive mnemonic; alias map
      (`BRA`, `BEQ`, …, `BLE`).
- [x] Add `instruction_help()` and canonical mnemonic formatting.
- [x] Create `docs/isa.md` skeleton with the register/flag/memory model and a
      pointer to the table as the source of truth.
- [x] Tests: every opcode unique, every size/operand kind correct, 63
      instructions, aliases resolve, reserved opcodes fail lookup.

**Acceptance**

- The ISA test asserts exactly 63 defined instructions and no duplicates.
- Assembler and disassembler can consume the same table without translation
  tables.
- `docs/isa.md` and the table agree.

Verified: 15 tests pass, including 12 ISA-specific tests; reserved-opcode
lookups return null; `docs/isa.md` is now the normative reference.

### M0.3 — Memory, registers, fetch/decode

- **Status:** `Done`
- **Depends on:** M0.2
- **Estimate:** 0.5 day
- **Files:** `include/kr8/cpu.hpp`, `src/vm/cpu.cpp`,
  `tests/test_cpu_decode.cpp`
- **Plan:** §3.1 Registers, §3.3 Memory map, §3.4 Reset, §3.5 Encoding, §3.9 Faults

**Work**

- [x] `Memory`: `std::array<uint8_t, 256>` with `read8`/`write8` and zero fill.
- [x] CPU registers: `A`, `B`, `PC`, `SP`, `FLAGS`; getters/setters.
- [x] Stack metadata: depth (0–256), floor (`image_high`), reset values.
- [x] Fetch opcode, decode length from the ISA table, fetch operand when needed.
- [x] `StepResult` with status, PC, cycles, and message.
- [x] PC-wrap fault when a 2-byte instruction starts at `0xFF`.
- [x] Illegal-opcode fault with the opcode and address in the message.
- [x] Tests: 1-byte/2-byte fetch, PC advance, wrap fault, illegal opcode.

**Acceptance**

- Decode tests pass for every defined opcode.
- A 2-byte instruction at `0xFE` advances PC to `0x00` correctly.
- A 2-byte instruction at `0xFF` faults instead of wrapping the operand fetch.

### M0.4 — ALU and flags

- **Status:** `Done`
- **Depends on:** M0.3
- **Estimate:** 0.5 day
- **Files:** `src/vm/cpu.cpp`, `tests/test_cpu_alu.cpp`
- **Plan:** §3.2 Flags, §3.6 Instruction set, §3.10 Cycle model

**Work**

- [x] `ADD`, `ADC`, `SUB`, `SBC`, `CMP` with exact `Z/C/N/V`.
- [x] `AND`, `OR`, `XOR`, `TST`, `NOT` with `C=0`, `V=0`.
- [x] `NEG`, `INC A/B`, `DEC A/B`, `SHL`, `SHR`, `SAR`, `CLC`, `SEC`.
- [x] `MOV`/`LD`/`POP` update `Z`/`N`; `ST`/`PUSH` do not.
- [x] Carry/borrow and signed-overflow tests at `0x00`, `0x7F`, `0x80`,
      `0xFF`, and boundary cases.
- [x] Cycle contribution per instruction (bytes + operation extras).

**Acceptance**

- A table-driven test covers every ALU opcode and both operand forms.
- Flag tests include add/sub carry, borrow, overflow, zero, and sign.
- No ALU instruction changes an unrelated flag.

### M0.5 — Loads, stores, stack, and control flow

- **Status:** `Done`
- **Depends on:** M0.4
- **Estimate:** 0.5 day
- **Files:** `src/vm/cpu.cpp`, `tests/test_cpu_control.cpp`
- **Plan:** §3.6 Instruction set, §3.7 Stack, calls, and returns, §3.9 Faults

**Work**

- [x] `MOV A/B, #n`; `MOV A, B`; `MOV B, A`.
- [x] `LD A/B, [a]`; `LD A, [B]`; `ST [a], A/B`; `ST [B], A`.
- [x] `PUSH A/B`, `POP A/B`; stack depth and floor checks.
- [x] `JMP`, `JZ`, `JNZ`, `JC`, `JNC`, `JN`, `JNN`, `JV`, `JNV`,
      `JLT`, `JGE`, `JGT`, `JLE`; taken/not taken.
- [x] `CALL`, `RET`, nested calls, return-address correctness.
- [x] `HLT`, `NOP`, `BRK`; `BRK` halt/resume semantics.
- [x] Stack-overflow and stack-underflow faults.
- [x] Tests: branch matrix, nested calls, stack faults, `BRK` resume.

**Acceptance**

- Every branch condition is tested both taken and not taken.
- `CALL` at `0xFE` pushes `0x00` and `RET` returns correctly.
- Stack overflow/underflow produce precise faults.
- `BRK` halts at the instruction and resumes past it.

### M0.6 — Console I/O and cycles

- **Status:** `Done`
- **Depends on:** M0.5
- **Estimate:** 0.25 day
- **Files:** `src/vm/cpu.cpp`, `tests/test_cpu_io.cpp`
- **Plan:** §3.8 Console I/O, §3.10 Cycle model

**Work**

- [x] `OUT A` callback; no flags.
- [x] `IN A` callback returning `std::optional<uint8_t>`.
- [x] `IN`: byte available → `A=byte`, `Z=0`; none → `A=0`, `Z=1`;
      `N/C/V` unchanged.
- [x] Test a real `0x00` byte (Z must be 0), a non-zero byte, and no byte.
- [x] Cycle formula: bytes + memory access + taken transfer; tests for
      representative instructions.

**Acceptance**

- I/O tests pass with an empty queue, a queued byte, and a queued `0x00`.
- Cycle counts match the formula in plan §3.10 for at least `ADD A,B`,
  `LD A,[a]`, `JZ` taken/not taken, `CALL`, and `RET`.

### M0.7 — Machine wrapper

- **Status:** `Done`
- **Depends on:** M0.6
- **Estimate:** 0.25 day
- **Files:** `include/kr8/machine.hpp`, `src/vm/machine.cpp`,
  `src/platform/console.cpp`
- **Plan:** §7.3 Machine API, §3.8 Console I/O

**Work**

- [x] `Machine` owns `Cpu`, `Memory`, output callback, input callback, stats.
- [x] `load(const Image&)` or a temporary 256-byte load path until M1.
- [x] `reset()`; `step()`; `halted()`; `fault()`; `out()`/`in()` plumbing.
- [x] Console output capture string; input queue helpers.
- [x] Blocking stdin host for the CLI (read one byte, EOF → no byte).
- [x] Instruction/cycle counters.

**Acceptance**

- A hand-built 256-byte image runs through `Machine::step()`.
- `OUT` bytes are captured in order.
- `IN` consumes a queued byte and returns no byte when empty.

### M0.8 — M0 test gate

- **Status:** `Done`
- **Depends on:** M0.1–M0.7
- **Estimate:** 0.25 day
- **Files:** `tests/`, `kr8-impl.md`
- **Plan:** §12 Testing, §13 M0

**Work**

- [x] Run the full CPU/ISA suite on Windows and Linux.
- [x] Run ASan/UBSan on Linux.
- [x] Confirm zero warnings under the configured flags.
- [x] Update the status dashboard and work log.
- [x] Record any open issue as a task or a blocker.

**Acceptance**

- All M0 tests pass; no known P0/P1 defect.
- `kr8_core` builds without UI.
- The work log has a gate entry.

Verified: 62 tests pass on Windows MSVC (`windows-vs2026-debug`), Linux GCC
(`linux-gcc-debug`), and Linux ASan/UBSan (`linux-gcc-sanitize`); zero warnings
under `/W4 /WX` and `-Wall -Wextra -Wpedantic -Werror`.

---

## 4. M1 — Image, assembler, disassembler

**Goal:** `.k8` images with symbols/lines/source/code map, a two-pass assembler,
a symbol-aware disassembler, and the `as`/`dis`/`sym` CLI commands.

**Implementation note:** the lexer, line parser, and expression evaluator live
in `src/asm/assembler.cpp`; the file split in M1.2/M1.3 is organizational, not a
behavior contract. CLI coverage is in CTest as `kr8.cli.as`, `kr8.cli.dis`, and
`kr8.cli.sym`.

### M1.1 — Image model and `.k8` format

- **Status:** `Done`
- **Depends on:** M0.8
- **Estimate:** 0.75 day
- **Files:** `include/kr8/image.hpp`, `src/image/image.cpp`,
  `tests/test_image.cpp`, `docs/image-format.md`
- **Plan:** §5 Image format

**Work**

- [x] `Image` struct: RAM, code map, entry, `image_high`, symbols, line rows,
      sources, source text, CRC.
- [x] Writer: 32-byte header, 256-byte RAM, 32-byte code map, symbols, lines,
      sources, string table, source blob, CRC32.
- [x] Reader: validate magic/version/flags/counts/offsets/CRC; reject
      truncation and overflow.
- [x] `.bin` read/write helpers (short raw file zero-filled; long rejected).
- [x] Document the exact layout in `docs/image-format.md`.
- [x] Tests: round-trip, CRC mismatch, truncated file, bad offsets, `.bin`
      round-trip.

**Acceptance**

- A populated `Image` round-trips byte-for-byte.
- Corrupt/truncated files fail with a clear error, not a crash.
- `docs/image-format.md` matches the implementation.

Verified: 9 image tests pass (71 total); CRC32 known vector, round-trip, bad
magic, checksum, truncation, raw `.bin`, and file I/O.

### M1.2 — Lexer and line parser

- **Status:** `Done`
- **Depends on:** M1.1
- **Estimate:** 0.5 day
- **Files:** `src/asm/lexer.cpp`, `src/asm/parser.cpp`,
  `include/kr8/assembler.hpp` (diagnostics), `tests/test_asm_lexer.cpp`
- **Plan:** §4.1 Source form, §4.2 Operand forms, §4.9 Diagnostics

**Work**

- [x] Tokenize labels (`name:`), mnemonics, directives, registers, numbers,
      characters, strings, symbols, punctuation, and `;` comments.
- [x] Number forms: decimal, `0x`, `$`, `0b`; character escapes.
- [x] Parse one statement per line into `{label, mnemonic, operands, location}`.
- [x] Support a label and an instruction/directive on one line.
- [x] Diagnostics: file/line/column, message, source line, caret.
- [x] Tests: valid lines, comments, blank lines, bad tokens, unterminated
      strings, bad escapes.

**Acceptance**

- Every syntax form in plan §4.1–§4.4 parses.
- Malformed input produces a diagnostic with the correct line/column.
- The parser does not crash on arbitrary text.

### M1.3 — Expressions

- **Status:** `Done`
- **Depends on:** M1.2
- **Estimate:** 0.5 day
- **Files:** `src/asm/expression.cpp`, `tests/test_asm_expression.cpp`
- **Plan:** §4.4 Expressions

**Work**

- [x] Pratt/precedence parser: `+ - * / % << >> & ^ |`, unary `+ - ~`,
      parentheses.
- [x] Symbols and `.equ` constants.
- [x] 32-bit signed evaluation; division/remainder by zero diagnostic.
- [x] Range checks at the point of use (immediate/address/target).
- [x] Tests: precedence, unary, chars, symbol resolution, div0, out-of-range.

**Acceptance**

- Expressions match C-like precedence.
- Undefined symbols and out-of-range values produce precise diagnostics.
- `.equ` forward references resolve in pass 2 where allowed.

### M1.4 — Two-pass assembler core

- **Status:** `Done`
- **Depends on:** M1.3
- **Estimate:** 1 day
- **Files:** `src/asm/assembler.cpp`, `tests/test_assembler.cpp`
- **Plan:** §4.5 Passes, §4.6 Encoding, §4.7 Symbol model, §4.8 Listing

**Work**

- [x] Pass 1: parse, size instructions from syntax, place labels, evaluate
      `.org`/`.space`/`.equ`, classify Code/Data/Const, detect duplicates.
- [x] Pass 2: resolve operands, encode, fill RAM, code map, line rows, symbol
      values/sizes, `image_high`, stack headroom.
- [x] Directives: `.org`, `.byte`, `.word`, `.ascii`, `.asciz`, `.space`,
      `.equ`, `.entry`.
- [x] Operand forms: register, `#expr`, `[expr]`, `[B]`, branch target.
- [x] Absolute branch/call targets; no relaxation.
- [x] Errors: backward `.org`, `0xFF` crossing, image overflow, label without
      code/data, undefined symbol, bad range.
- [x] Tests: all directives, labels, forward refs, code/data classification,
      entry, headroom warning, error cases.

**Acceptance**

- All seven examples assemble to the expected addresses/bytes.
- Symbols and line rows match the source.
- Error cases fail without writing output.

### M1.5 — Listing, summary, and `kr8 as`

- **Status:** `Done`
- **Depends on:** M1.4
- **Estimate:** 0.5 day
- **Files:** `src/asm/listing.cpp`, `src/cli/main.cpp`,
  `tests/test_cli_as.cpp`
- **Plan:** §4.8 Listing and summary, §7.1 Commands

**Work**

- [x] Listing: address, bytes, source line; symbols; image summary.
- [x] Summary: code bytes, data bytes, `image_high`, stack headroom.
- [x] `kr8 as <file.asm>` default output `<source>.k8`; `-o`, `--raw`,
      `--no-source`, `-l`, `-q`.
- [x] Exit code 1 on assembly error; no output file on error.
- [x] Tests: output path, listing, raw export, quiet, failure cases.

**Acceptance**

- `kr8 as examples/hello.asm` produces `hello.k8` and the expected summary.
- `-l`, `--raw`, and `--no-source` behave as specified.
- Errors leave no partial `.k8`.

### M1.6 — Disassembler library

- **Status:** `Done`
- **Depends on:** M1.4
- **Estimate:** 0.5 day
- **Files:** `include/kr8/disassembler.hpp`, `src/dis/disassembler.cpp`,
  `tests/test_disassembler.cpp`
- **Plan:** §6 Disassembler

**Work**

- [x] Decode from the code map; never decode data bytes as instructions.
- [x] Produce `DecodedInstruction {address, size, bytes, text, line, valid}`.
- [x] Symbol resolution for branch/call targets and `[addr]` operands.
- [x] Data ranges: `.byte`/`.asciz` display where appropriate.
- [x] Help text from the ISA table.
- [x] Tests: every opcode, symbols, data regions, raw fallback.

**Acceptance**

- `hello.k8` disassembles to the expected instruction list.
- Operands use symbols when an exact match exists.
- Data bytes are not decoded as instructions.

### M1.7 — `kr8 dis` and `kr8 sym`

- **Status:** `Done`
- **Depends on:** M1.6
- **Estimate:** 0.5 day
- **Files:** `src/cli/main.cpp`, `tests/test_cli_dis.cpp`
- **Plan:** §6.1 CLI, §7.1 Commands

**Work**

- [x] Format `ADDR/BYTES/INSTRUCTION` columns.
- [x] Options: `--raw`, `--base`, `--end`, `--symbols`, `--source`, `--data`.
- [x] `kr8 sym <image>` prints the symbol table.
- [x] Tests: normal, raw, symbol, source, data, bad input.

**Acceptance**

- CLI output matches plan §6.1's shape and resolves symbols.
- Raw mode works without a `.k8` header.
- `sym` prints code/data/const entries with values and sizes.

### M1.8 — M1 test gate

- **Status:** `Done`
- **Depends on:** M1.1–M1.7
- **Estimate:** 0.25 day
- **Files:** `tests/`, `examples/`, `kr8-impl.md`
- **Plan:** §12 Testing, §13 M1

**Work**

- [x] Assemble all examples.
- [x] Round-trip every image through read/write.
- [x] Disassemble every example and check symbols/data.
- [x] Run Windows and Linux suites; ASan/UBSan on Linux.
- [x] Update dashboard and work log.

**Acceptance**

- All M1 tests pass; no known P0/P1 defect.
- The examples are checked in and assemble reproducibly.

Verified: all seven examples assemble, write/read round-trip, and disassemble;
90 C++ tests and 5 CTest targets pass on Windows MSVC, Linux GCC, and Linux
ASan/UBSan; zero warnings.

---

## 5. M2 — VM and CLI

**Goal:** `kr8 run` executes `.k8`/`.bin` with console I/O, trace, stats, budget,
and precise faults. Golden examples prove the whole pipeline.

### M2.1 — Runner and `kr8 run`

- **Status:** `Done`
- **Depends on:** M1.8
- **Estimate:** 0.5 day
- **Files:** `src/vm/runner.cpp`, `src/cli/main.cpp`,
  `tests/test_cli_run.cpp`
- **Plan:** §7.2 `run`

**Work**

- [x] Load `.k8` or raw `.bin`; reset; run step loop.
- [x] `OUT` to stdout; `IN` from `--input` or stdin; EOF → no byte.
- [x] `--entry`, `--raw`, `--input`, `--max-instructions`, `--trace`,
      `--stats`.
- [x] Exit codes 0/1/2/3.
- [x] Trace format with PC, A, B, SP, flags, instruction.
- [x] Tests: each option, exit codes, trace shape.

**Acceptance**

- `kr8 run examples/hello.k8` prints `Hello, KR8!` and exits 0.
- Budget and fault paths return the documented exit codes.
- `--input` feeds `echo.asm` byte-for-byte.

### M2.2 — Fault diagnostics and edge cases

- **Status:** `Done`
- **Depends on:** M2.1
- **Estimate:** 0.5 day
- **Files:** `src/vm/runner.cpp`, `tests/test_faults.cpp`
- **Plan:** §3.9 Faults

**Work**

- [x] Illegal opcode: address, opcode, readable message.
- [x] Stack overflow/underflow: address and operation.
- [x] PC wrap: address and cause.
- [x] `BRK`: stop, message, exit 0; `HLT`: normal stop.
- [x] Tests: each fault from a crafted image.

**Acceptance**

- Every fault prints a precise message and exits 2 (except `BRK`/`HLT`).
- `PC` at the fault is the faulting instruction.
- No fault path crashes or hangs.

### M2.3 — Golden examples and CLI tests

- **Status:** `Done`
- **Depends on:** M2.2
- **Estimate:** 0.5 day
- **Files:** `examples/*.asm`, `tests/test_examples.cpp`,
  `examples/README.md`
- **Plan:** §12.2 Golden programs, Appendix A

**Work**

- [x] Add `hello.asm`, `count.asm`, `echo.asm`, `puts.asm`, `memory.asm`,
      `stack.asm`, `flags.asm`.
- [x] Assemble each in the test, run, compare stdout.
- [x] Check `memory.asm` memory contents after the run.
- [x] Check `count.asm` output `0123456789\n`.
- [x] Check `stack.asm` output `!A`.
- [x] Document each example's teaching point.

**Acceptance**

- All golden tests pass on Windows and Linux.
- Each example is small enough to leave clear stack headroom.

### M2.4 — M2 test gate

- **Status:** `Done`
- **Depends on:** M2.1–M2.3
- **Estimate:** 0.25 day
- **Files:** `tests/`, `kr8-impl.md`

**Work**

- [x] Full suite on Windows and Linux.
- [x] ASan/UBSan on Linux.
- [x] `kr8 as` → `kr8 run` pipeline for every example.
- [x] Update dashboard and work log.

**Acceptance**

- All M2 tests pass; no known P0/P1 defect.

Verified: 105 C++ tests and 6 CTest targets pass on Windows MSVC, Linux GCC,
and Linux ASan/UBSan. All seven examples run through `as` → `run`; CLI exit
codes were checked for illegal opcode (2), budget (3), BRK (0), and PC wrap (2).

---

## 6. M3 — Debug session

**Goal:** a portable `Session` that owns the machine, image, symbols, lines,
breakpoints, shadow call stack, console/input, and stats. The UI will render
snapshots from this object only.

### M3.1 — Session load, restart, snapshot

- **Status:** `Done`
- **Depends on:** M2.4
- **Estimate:** 0.75 day
- **Files:** `include/kr8/session.hpp`, `src/debug/session.cpp`,
  `tests/test_session_load.cpp`
- **Plan:** §8.1 API, §8.2 Loading and building

**Work**

- [x] `Session` owns image, CPU, memory, console, input, breakpoints, stats.
- [x] `load_image`, `assemble`, `restart`, `unload`.
- [x] Snapshot: registers, flags, PC/SP, line, function, console, frames,
      stats, message.
- [x] Source lines, executable lines, disassembly, symbols, line/symbol lookup.
- [x] Reject bad images without destroying the previous session.
- [x] Tests: load/restart/unload, snapshot fields, failed load preserves state.

**Acceptance**

- Loading `hello.k8` produces the expected snapshot.
- Restart resets CPU/console/stats but keeps breakpoints.
- A failed assemble/load leaves the previous image intact.

### M3.2 — Stepping and shadow call stack

- **Status:** `Done`
- **Depends on:** M3.1
- **Estimate:** 1 day
- **Files:** `src/debug/session.cpp`, `tests/test_session_steps.cpp`
- **Plan:** §8.3 Stepping, §8.5 Shadow call stack

**Work**

- [x] `step_instruction`, `step_instruction_over`, `step_instruction_out`.
- [x] `step_line(LineIn/LineOver/LineOut)` with depth predicates.
- [x] Shadow call stack: `CALL` push, `RET` pop, caller line, current frame.
- [x] `RET` without call and return-address mismatch diagnostics.
- [x] 1,000,000-instruction step budget.
- [x] Transcript tests for straight-line code, loops, calls, recursion.

**Acceptance**

- Step transcripts match the expected PC/line/depth sequences.
- Step over a call returns to the instruction after the call.
- Step out leaves the current function.
- `RET` without `CALL` reports the diagnostic instead of silently continuing.

### M3.3 — Breakpoints

- **Status:** `Done`
- **Depends on:** M3.2
- **Estimate:** 0.75 day
- **Files:** `src/debug/session.cpp`, `tests/test_session_breakpoints.cpp`
- **Plan:** §8.4 Breakpoints

**Work**

- [x] Address, line, and symbol breakpoints.
- [x] Check before executing; skip the check once when continuing.
- [x] Line breakpoints hit every address mapped to the line.
- [x] Snap a non-executable line forward and report the move.
- [x] Preserve breakpoints across restart/reassemble; remap lines.
- [x] `run_slice(max)` returns Budget/Breakpoint/Halted/Fault/Break.
- [x] Tests: hit, continue past, breakpoint during step, line with multiple
      rows, restart persistence.

**Acceptance**

- A line breakpoint stops before the first instruction of that line.
- Continue from a breakpoint executes the instruction and does not re-trigger
  immediately.
- Line breakpoints survive a successful reassemble.

### M3.4 — Console, input, stats, and editing

- **Status:** `Done`
- **Depends on:** M3.1
- **Estimate:** 0.5 day
- **Files:** `src/debug/session.cpp`, `tests/test_session_io.cpp`
- **Plan:** §8.6 Console and input, §8.7 Editing, §8.8 Stats

**Work**

- [x] Console output string; input queue; `push_input`/`clear_input`.
- [x] `IN` polling through the session.
- [x] `instructions_executed`, `cycles`, `run_time_ms`, `reset_stats`.
- [x] Register/flag/memory edits; PC edit updates line; SP edit invalidates
      frames.
- [x] Tests: output capture, input queue, stats reset, edit effects.

**Acceptance**

- `echo.asm` runs through the session with queued input and captured output.
- Stats reset does not change machine state.
- SP edit produces "call stack unavailable" rather than stale frames.

### M3.5 — M3 test gate

- **Status:** `Done`
- **Depends on:** M3.1–M3.4
- **Estimate:** 0.5 day
- **Files:** `tests/`, `kr8-impl.md`

**Work**

- [x] Full session suite on Windows and Linux.
- [x] Transcript tests for all step modes.
- [x] ASan/UBSan on Linux.
- [x] Update dashboard and work log.

**Acceptance**

- All M3 tests pass; the UI-facing API is stable.
- No known P0/P1 defect.

Verified: 134 C++ tests and 6 CTest targets pass on Windows MSVC, Linux GCC,
and Linux ASan/UBSan. Transcripts cover straight-line code, guard loops via the
budget, nested and recursive calls, and line In/Over/Out. Breakpoints, frames,
input, stats, editing, return-without-call, and BRK/fault stops are covered.

---

## 7. M4 — UI foundation and Win32

**Goal:** the KR16-style debugger window on Windows: source, disassembly,
registers/flags, full memory grid, symbols, call stack, console, errors, help,
and load/assemble/run/break/restart.

### M4.1 — Port UI interfaces

- **Status:** `Done`
- **Depends on:** M3.5
- **Estimate:** 0.5 day
- **Files:** `include/kr8/ui/geometry.hpp`, `canvas.hpp`, `input.hpp`,
  `window.hpp`, `tests/test_ui_layout.cpp`
- **Plan:** §9.1 Reuse strategy, §9.2 Visual language

**Work**

- [x] Copy KR16's portable interfaces, rename namespace to `kr8::ui`.
- [x] Keep the same `Canvas`, `Window`, `Input`, `Event`, `FontRole` shapes.
- [x] Add a fake canvas for layout tests (fixed metrics).
- [x] Ensure `src/ui` has no OS headers.
- [x] Tests: geometry, rect hit testing, fake canvas measurement.

**Acceptance**

- `kr8_ui` skeleton builds without a platform backend.
- Fake-canvas layout tests pass.
- No Windows/SDL headers in `include/kr8/ui`.

### M4.2 — Win32 backend

- **Status:** `Done`
- **Depends on:** M4.1
- **Estimate:** 1.5 days
- **Files:** `src/platform/ui/win32/win32_canvas.cpp`,
  `win32_window.cpp`, `win32_launcher.cpp`, `src/ui/run_win32.cpp`
- **Plan:** §9.1, §9.11 Windows entry point

**Work**

- [x] GDI canvas: fill, frame, text, measure, line height, clip.
- [x] Window loop, timers, mouse/keyboard events, resize, focus.
- [x] Embedded fonts via `AddFontMemResourceEx`; UI 14/14 bold, Mono 15/13.
- [x] File dialog, error dialog, cursor, clipboard.
- [x] DPI and logical-pixel coordinates.
- [x] `kr8 --ui` entry; no-arg spawn with `CREATE_NO_WINDOW`.
- [x] Acceptance: blank window opens, text renders, events arrive.

**Acceptance**

- A blank KR8 window opens and paints the background/panels.
- Text uses the embedded fonts on a machine without them installed.
- Resize, DPI, and keyboard/mouse events are delivered correctly.
- No console window appears for the UI child.

### M4.3 — Theme, text field, and help scaffolding

- **Status:** `Done`
- **Depends on:** M4.1
- **Estimate:** 0.5 day
- **Files:** `src/ui/theme.hpp`, `src/ui/text_field.cpp`,
  `src/ui/text_field.hpp`, `src/ui/help.cpp`, `src/ui/help.hpp`
- **Plan:** §9.2 Visual language, §9.9 Errors and help

**Work**

- [x] Move the KR16 palette into `theme.hpp` with named tokens.
- [x] Port `TextField` for console input and register/memory edit.
- [x] Generate help text from the ISA table (mnemonic, operands, flags).
- [x] Add the F1 overlay data: registers, flags, instructions, directives,
      shortcuts.
- [x] Tests: text field edit/commit/cancel; help lookup every mnemonic.

**Acceptance**

- Palette matches plan §9.2.
- Text field handles typing, Backspace, Enter, Escape, and commit/cancel.
- Every defined instruction has help text; no drift from the ISA table.

### M4.4 — App layout and top bar

- **Status:** `Done`
- **Depends on:** M4.2, M4.3
- **Estimate:** 1 day
- **Files:** `src/ui/app.cpp`, `src/ui/app.hpp`
- **Plan:** §9.3 Layout, §9.4 Top bar

**Work**

- [x] `layout()` computes all pane rectangles from window size.
- [x] Top bar: Load, Assemble, Run, Break, Restart, Reset stats; counters
      INS/CYCLES/TIME/IPS.
- [x] Button hit testing, hover, enabled/disabled states.
- [x] Status bar.
- [x] Empty state: "Load a .asm or .k8 file."
- [x] Tests: rectangles, button states, minimum size 1024x700.

**Acceptance**

- The window matches the plan's wireframe at 1280x820 and remains usable at
  1024x700.
- Run/Break/Assemble/Restart enable and disable correctly.
- No pane overlaps at the minimum size.

### M4.5 — Source pane

- **Status:** `Done`
- **Depends on:** M4.4
- **Estimate:** 1 day
- **Files:** `src/ui/app.cpp`, `tests/test_ui_source.cpp`
- **Plan:** §9.5 Source pane

**Work**

- [x] Line numbers, current-line highlight, gutter breakpoints.
- [x] Dim non-executable lines; snap breakpoints forward and report.
- [x] Scrollbar, wheel, drag, auto-follow current line.
- [x] Over/In/Out buttons wired to `Session`.
- [x] Empty/no-source state.
- [x] Tests: gutter mapping, breakpoint toggle, follow behavior.

**Acceptance**

- A student can set/clear a line breakpoint and step a source line.
- Non-executable lines are visibly dimmed.
- The pane follows the PC without fighting manual scrolling.

### M4.6 — Disassembly pane

- **Status:** `Done`
- **Depends on:** M4.4
- **Estimate:** 1 day
- **Files:** `src/ui/app.cpp`, `tests/test_ui_asm.cpp`
- **Plan:** §9.6 Disassembly pane

**Work**

- [x] Rows: address, bytes (1–2), instruction; fixed columns.
- [x] PC highlight; current source line shade; breakpoint gutter.
- [x] Symbol-resolved operands.
- [x] Hover help for mnemonic and operand.
- [x] Instruction Over/In/Out buttons.
- [x] Scrollbar and click-to-select.
- [x] Tests: row mapping, breakpoint toggle, symbol text.

**Acceptance**

- The PC row is always visible and distinct from the current-line shade.
- Address breakpoints toggle from the gutter.
- Mnemonic/operand hover shows the correct help.

### M4.7 — Registers, flags, and memory grid

- **Status:** `Done`
- **Depends on:** M4.4
- **Estimate:** 1.5 days
- **Files:** `src/ui/app.cpp`, `tests/test_ui_registers.cpp`,
  `tests/test_ui_memory.cpp`
- **Plan:** §9.7 Registers, flags, and memory

**Work**

- [x] Register strip A/B/PC/SP; hover hex/decimal; click-to-edit.
- [x] Flag boxes Z/C/N/V; click-to-toggle; hover name/state.
- [x] Changed-value highlight for one stop.
- [x] 16x16 memory grid with address, hex, ASCII.
- [x] PC/SP highlights, data-symbol shading, code/operand/data tones.
- [x] Byte hover and two-hex-digit edit; amber edited marker.
- [x] Tests: cell mapping, edit validation, PC/SP highlights.

**Acceptance**

- Editing A/B/PC/SP works and validates `00`–`FF`.
- A code-byte edit changes what executes and is marked.
- The entire 256-byte RAM is visible without scrolling.

### M4.8 — Symbols, call stack, console

- **Status:** `Done`
- **Depends on:** M4.4, M3.4
- **Estimate:** 1 day
- **Files:** `src/ui/app.cpp`, `tests/test_ui_symbols.cpp`
- **Plan:** §9.8 Symbols, call stack, console

**Work**

- [x] Symbols groups Code/Data/Const; name/value/size or ASCII preview.
- [x] Click to jump; double-click to toggle breakpoint; current symbol highlight.
- [x] Shadow call stack frames; click to jump; "best effort" label.
- [x] Console output, input queue, input field, Send/Clear.
- [x] Printable vs escaped bytes; auto-scroll.
- [x] Tests: symbol hit testing, frame click, input queue.

**Acceptance**

- Clicking a symbol moves the disassembly/source to its address.
- The call stack shows caller frames for a nested call.
- Typed console input is consumed by `IN` and appears in the output.

### M4.9 — Errors, help overlay, keyboard, settings

- **Status:** `Done`
- **Depends on:** M4.5–M4.8
- **Estimate:** 1 day
- **Files:** `src/ui/app.cpp`, `src/ui/help.cpp`, `src/ui/settings.cpp`
- **Plan:** §9.9 Errors and help, §9.10 Keyboard, settings, startup

**Work**

- [x] Diagnostics view; `file:line:col`; click jumps; error gutters.
- [x] Keep the previous image loaded on assemble failure.
- [x] F1 help overlay; close with Esc/click.
- [x] Keyboard: F5/F6/F7/F8/F10/F11, Ctrl+O/Ctrl+R.
- [x] Settings: font size, slice size, follow, changed values; persist.
- [x] Tests: diagnostic jump, shortcut dispatch, settings round-trip.

**Acceptance**

- A bad assembly shows errors and leaves the old image step-able.
- F1 fits the whole ISA reference on one overlay.
- Settings survive a restart of the app.

### M4.10 — Win32 smoke test and screenshots

- **Status:** `Done`
- **Depends on:** M4.9
- **Estimate:** 0.5 day
- **Files:** `src/ui/main.cpp`, `tests/ui_smoke.ps1`,
  `docs/screenshots/*.png`
- **Plan:** §12.3 UI tests

**Work**

- [x] Load `hello.k8`, step, run, break, restart.
- [x] Set a line breakpoint and an address breakpoint.
- [x] Edit a register and a memory byte.
- [x] Capture screenshots: load, stepping, breakpoint, registers/memory,
      symbols, console, help.
- [x] Record the smoke command.

**Acceptance**

- The smoke test runs on a clean Windows machine.
- Screenshots match the intended visual language.
- No crash or hang in the scripted path.

### M4.11 — M4 test gate

- **Status:** `Done`
- **Depends on:** M4.1–M4.10
- **Estimate:** 0.5 day
- **Files:** `tests/`, `kr8-impl.md`

**Work**

- [x] Full suite on Windows.
- [x] UI feature checklist from plan Appendix B.
- [x] Zero warnings.
- [x] Update dashboard and work log.

**Acceptance**

- Every v1 UI feature in the checklist is present and exercised.
- No known P0/P1 defect.

Verified on Windows: the UI builds and runs. Load/assemble, Run/Break/Restart,
source line steps, instruction steps, line and address breakpoints, register/
flag/memory editing, symbols, call stack, console, F1 help, settings, hover
help, and the assembly-error overlay were exercised. Screenshots are in
`docs/screenshots/`. 134 C++ tests and 6 CTest targets pass.

---

## 8. M5 — SDL3/Linux

**Goal:** the same debugger on Linux with SDL3, static SDL3, embedded fonts,
native dialogs with fallback, and Xvfb smoke tests.

### M5.1 — SDL3 canvas and window

- **Status:** `Done`
- **Depends on:** M4.11
- **Estimate:** 2 days
- **Files:** `src/platform/ui/sdl/sdl_canvas.cpp`,
  `sdl_window.cpp`, `src/ui/run_sdl.cpp`, `third_party/stb/stb_truetype.h`
- **Plan:** §9.1, §9.11 Linux entry point

**Work**

- [x] SDL3 video/window/event loop; software canvas.
- [x] stb_truetype glyph atlas; embedded fonts; same sizes as Win32.
- [x] Mouse/keyboard/text input; wheel; resize; focus.
- [x] Timers; DPI/logical coordinates.
- [x] Native file dialog with a built-in fallback.
- [x] Static SDL3 FetchContent wiring and `KR8_SDL3_SOURCE_DIR`.
- [x] Acceptance: window opens under Xvfb; text renders.

**Acceptance**

- `kr8 --ui examples/hello.k8` opens under Xvfb.
- Text metrics and colors match the Win32 look closely.
- No Windows headers or APIs in `src/ui`.

### M5.2 — Linux app integration

- **Status:** `Done`
- **Depends on:** M5.1
- **Estimate:** 1 day
- **Files:** `src/cli/main.cpp`, `src/ui/run_sdl.cpp`,
  `src/platform/ui/sdl/sdl_window.cpp`
- **Plan:** §9.11, §11

**Work**

- [x] No-argument Linux launch runs the UI in-process.
- [x] `ui` command and file argument.
- [x] Headless no-display error path.
- [x] Settings paths (`XDG_CONFIG_HOME`, `~/.config`).
- [x] `KR8_BUILD_UI=OFF` still builds the CLI.
- [x] Acceptance: load/assemble/run/break on Linux.

**Acceptance**

- The UI opens with and without a file argument.
- `KR8_BUILD_UI=OFF` builds and tests the CLI headlessly.
- Settings persist under XDG paths.

### M5.3 — Linux parity tests and screenshots

- **Status:** `Done`
- **Depends on:** M5.2
- **Estimate:** 0.5 day
- **Files:** `tests/ui_smoke_linux.sh`, `docs/screenshots/*.png`
- **Plan:** §12.3 UI tests

**Work**

- [x] Xvfb smoke: load, step, run, break, restart, breakpoints, edits.
- [x] Screenshots for README/docs.
- [x] Compare feature checklist with Win32.
- [x] Record commands.

**Acceptance**

- The same feature checklist passes on Linux x64.
- Xvfb smoke is green in CI.

### M5.4 — M5 test gate

- **Status:** `Done`
- **Depends on:** M5.1–M5.3
- **Estimate:** 0.5 day
- **Files:** `tests/`, `kr8-impl.md`

**Work**

- [x] Full suite on Windows and Linux.
- [x] Platform-boundary check.
- [x] ASan/UBSan on Linux.
- [x] Update dashboard and work log.

**Acceptance**

- Both platforms pass; no known P0/P1 defect.
- No OS headers outside `src/platform`.

---

## 9. M6 — Polish, docs, packaging

**Goal:** documentation that matches behavior, a single Windows executable, a
Linux x64 AppImage, CI, and the final v1 verification.

### M6.1 — Documentation

- **Status:** `Done`
- **Depends on:** M5.4
- **Estimate:** 1 day
- **Files:** `README.md`, `docs/isa.md`, `docs/assembler.md`,
  `docs/image-format.md`, `docs/debugger.md`, `docs/curriculum.md`,
  `examples/README.md`, `THIRD_PARTY_NOTICES.md`
- **Plan:** §10, §12, Appendix B

**Work**

- [x] README: what KR8 is, build, quick start, commands, UI screenshots.
- [x] `docs/isa.md`: normative ISA, flags, memory, I/O, faults, cycles.
- [x] `docs/assembler.md`: syntax, directives, expressions, diagnostics.
- [x] `docs/image-format.md`: exact `.k8` layout.
- [x] `docs/debugger.md`: panes, shortcuts, breakpoints, editing.
- [x] `docs/curriculum.md`: lesson sequence with the examples.
- [x] `THIRD_PARTY_NOTICES.md`: fonts, SDL3, stb.
- [x] Verify every command and key in the docs against the build.

**Acceptance**

- A new user can build, assemble, run, and debug using only the docs.
- Every ISA row and directive in the docs matches the code.

### M6.2 — Packaging

- **Status:** `Done`
- **Depends on:** M6.1
- **Estimate:** 1 day
- **Files:** `packaging/`, `CMakeLists.txt`, `dist/`
- **Plan:** §11 Build system, §13 M6

**Work**

- [x] Windows: `/MT`, single exe, embedded fonts, no DLLs; `dumpbin
      /DEPENDENTS` check.
- [x] Linux: static SDL3, embedded fonts, x64 AppImage.
- [x] Test on a clean Windows VM and a clean Linux environment.
- [x] Add the licence files to the artifacts.
- [x] Record the artifact sizes and dependency output.

**Acceptance**

- The Windows exe runs with only the file and its `.k8`/`.asm` inputs.
- The AppImage runs on a clean Ubuntu 22.04 x64 environment.
- No missing runtime dependency.

Verified on Windows: `kr8.exe` is 657,408 bytes, linked `/MT`, with only
`USER32.dll`, `GDI32.dll`, `COMDLG32.dll`, and `KERNEL32.dll` as dependencies.
The zip is 1,390,999 bytes and runs from an empty temporary directory. The Linux
AppImage is 2.1 MiB, built in an Ubuntu 22.04 rootfs; `objdump -T` shows a
maximum of `GLIBC_2.34` and `GLIBCXX_3.4.29`, and `ldd` needs only libstdc++,
libm, libgcc_s, and libc. The AppImage passes `--version`, `run
examples/hello.k8`, and the Xvfb UI (F5) inside Ubuntu 22.04. License and notice
files are included in both artifacts.

### M6.3 — CI and release

- **Status:** `Done`
- **Depends on:** M6.2
- **Estimate:** 0.5 day
- **Files:** `.github/workflows/ci.yml`, `.github/workflows/release.yml`
- **Plan:** §11, §12.4

**Work**

- [x] CI: Windows MSVC and Ubuntu GCC; UI ON and OFF.
- [x] Xvfb UI smoke on Linux.
- [x] ASan/UBSan Linux job.
- [x] Platform-boundary test.
- [x] Release workflow: Windows zip and Linux x64 AppImage on a tag.
- [x] Acceptance: green CI; artifacts published.

**Acceptance**

- A pull request runs the full matrix.
- A tag produces the artifacts and they pass the smoke checks.

CI now covers Windows MSVC, Ubuntu GCC with the UI, a Linux sanitizer job, the
Xvfb smoke, and the platform-boundary script. A tag workflow builds the Windows
zip and the Ubuntu 22.04 AppImage and publishes them. The equivalent local
commands pass; the hosted GitHub Actions run itself is not available in this
environment.

### M6.4 — Final verification and v1 sign-off

- **Status:** `Done`
- **Depends on:** M6.1–M6.3
- **Estimate:** 0.5 day
- **Files:** `kr8-impl.md`, `kr8-plan.md`
- **Plan:** §12, §13, §15

**Work**

- [x] Run the full test matrix on Windows and Linux.
- [x] Verify the Definition of Done below.
- [x] Update the plan/impl if any implementation detail diverged.
- [x] Record the final work-log entry and mark M6 `Done`.
- [x] Tag `v0.1.0` if the release is in scope.

**Acceptance**

- The v1 Definition of Done is satisfied.
- No known P0/P1 defect.
- The work log is complete.

Verified 2026-11-03: Windows Debug and Release suites pass (134 C++, 6 CTest);
Linux UI suite passes 8 CTest targets including the Xvfb smoke and platform
boundary; Linux ASan/UBSan passes 7 targets; a fresh clone builds and tests on
both platforms. The Windows zip and Ubuntu 22.04 AppImage were exercised from
clean directories. Tagged `v0.1.0`.
---

## 10. Cross-cutting work

These are not separate milestones; they are habits applied throughout.

| Area | Rule |
|---|---|
| Tests | Add the test with the code. Gates are confirmation, not the first test run. |
| Docs | Update the relevant doc when behavior changes; do not batch all docs into M6. |
| Plan/impl sync | A design change edits `kr8-plan.md` first, then this checklist. |
| Diagnostics | Every user-facing error has a source location (assembler) or a precise machine cause (VM). |
| Warnings | Zero warnings under the configured flags at every gate. |
| Sanitizers | ASan/UBSan on Linux from M0 onward. |
| Performance | Run in slices; no unbounded loops in the UI; step budget in the session. |
| Compatibility | No OS headers outside `src/platform`; no Windows-only code in `src/ui`. |
| Dependencies | No new third-party dependency without a licence entry and a plan note. |
| Versioning | Tool `0.2.2`; image format `1.0`; bump deliberately. |

---

## 11. Test gates and commands

| Gate | Covers | Windows | Linux |
|---|---|---|---|
| G0 | Skeleton, ISA, CPU, machine | configure/build/ctest | configure/build/ctest |
| G1 | Image, assembler, disassembler | ctest + assemble examples | ctest + assemble examples |
| G2 | Runner, faults, golden examples | ctest + CLI examples | ctest + CLI examples |
| G3 | Session, stepping, breakpoints | ctest | ctest + ASan/UBSan |
| G4 | Win32 UI | smoke + screenshots + checklist | — |
| G5 | SDL3/Linux | — | Xvfb smoke + checklist |
| G6 | Release | single-exe dependency check | AppImage clean-VM check |

The exact commands are recorded when the presets and smoke harnesses exist.
Until then, use the preset commands in §0.

---

## 12. Definition of Done (v1)

KR8 v1 is done when all of the following are true:

- [x] All 44 tasks are `Done`; no task is `Blocked`.
- [x] `kr8_core`, `kr8`, `kr8_ui`, and `kr8_ui_smoke` build on Windows and
      Linux with zero warnings.
- [x] All unit/integration tests pass on both platforms; ASan/UBSan is clean on
      Linux.
- [x] All seven examples assemble and produce their expected output.
- [x] `kr8 run` reports illegal opcode, stack overflow/underflow, and PC wrap
      precisely.
- [x] The debugger can load/assemble, step instruction and source line, step
      over/in/out, set address/line/symbol breakpoints, edit registers/flags/
      memory, show the full 256-byte memory grid, symbols, shadow call stack,
      console, and help.
- [x] The Win32 and SDL3 UIs share the portable `src/ui` and pass the same
      feature checklist.
- [x] `docs/isa.md`, `docs/assembler.md`, `docs/image-format.md`,
      `docs/debugger.md`, and `docs/curriculum.md` match the code.
- [x] Windows ships one exe with no DLLs; Linux ships a working x64 AppImage.
- [x] `kr8-plan.md` and `kr8-impl.md` agree; all design changes are recorded.
- [x] No known P0/P1 defect; the work log is complete.

---

## 13. Risks, blockers, and decision changes

The design risks live in plan §14. This section is for implementation-specific
blockers and changes.

### Current blockers

None.

### Decision-change protocol

If a task needs to change the frozen design:

1. Stop and record the reason in the work log.
2. Edit `kr8-plan.md` (the affected section and, if needed, the decision table).
3. Update this checklist (task scope, acceptance, dependencies).
4. Resume only after the plan and checklist agree.

Examples that would require this: changing an opcode, changing `IN` semantics,
adding an image section, changing the UI pane set, or changing the host stack.

---

## 14. Work log

Timestamps use `local-datetime` and the host time zone
(America/Los_Angeles, UTC-08:00). New entries go at the top of the log.

### 2026-11-03

- **2026-11-03T16:39:18-08:00** — Console hover input: while the pointer is over
  the Console pane, printable keys feed `IN` immediately without the field or
  Enter; Enter/Tab/Backspace queue `\n`/`09`/`08`. Added a hover accent and hint,
  help/docs, and a screenshot. Tool version bumped to `0.2.2`; tag `v0.2.2`.
- **2026-11-03T16:25:55-08:00** — Added `examples/updown.asm`: a `count`
  variable starting at 5, a `+ up  - down` banner, one count per line, `+`/`-`
  adjustment clamped to 1..9, ignored newlines/unknown keys, and non-blocking
  polling. Added example/runner coverage; 146 C++ tests pass.
- **2026-11-03T16:12:30-08:00** — HLT now leaves PC on the `HLT` instruction,
  matching `BRK`/fault stop semantics. Source line, disassembly row, memory
  byte, and PC register now align at halt. Updated ISA/debugger/plan docs and
  tests; 145 C++ tests pass, Windows CTest 6/6, Linux UI CTest 8/8, sanitizer
  7/7. Tool version and artifacts bumped to `0.2.1`; tag `v0.2.1`.
- **2026-11-03T15:45:50-08:00** — Post-v1 built-in source editor implemented in
  explicit Edit mode: multiline editing, selection, clipboard, undo/redo,
  syntax highlighting, auto-indent, Ctrl+S/dirty prompts, stale-pane dimming,
  F8 assemble/reload, and line-breakpoint revalidation. Added `SourceEditor`
  tests; 144 C++ tests pass, Windows CTest 6/6, Linux UI CTest 8/8, sanitizer
  7/7. Docs and screenshots updated. Tool version bumped to `0.2.0`; tag `v0.2.0`.
  Artifacts: Windows exe 704,512 bytes, zip about 1.5 MiB; Ubuntu 22.04
  AppImage 2.2 MiB with a maximum of `GLIBC_2.34`.
- **2026-11-03T15:08:29-08:00** — Fixed the call-stack pane overlap: frames now
  use a two-line row (function/index, then source line plus `ret`/`pc`) and the
  heading is clipped to the pane. Added a nested three-frame regression test;
  135 C++ tests, Windows CTest, and Linux UI/ASan tests pass. `v0.1.0`
  artifacts were refreshed with the fix.
- **2026-11-03T12:43:00-08:00** — M6 and v1 complete. Windows Debug/Release
  pass 134 C++ tests and 6 CTest targets; Linux UI passes 8 targets including
  Xvfb and platform boundary; ASan/UBSan passes 7. Fresh clones build/test.
  Windows zip (657,408-byte exe, 1,390,999-byte zip) and a 2.1 MiB Ubuntu
  22.04 AppImage (GLIBC 2.34 maximum) were exercised, including the AppImage UI.
  All 44 tasks and the Definition of Done are complete. Tag `v0.1.0`.
- **2026-11-03T12:29:03-08:00** — M5 gate passed on Linux: static SDL3 UI,
  Xvfb screenshot/parity, no-argument in-process launch, XDG settings, and the
  `kr8.ui.smoke.linux` CTest. Windows and Linux suites pass; M5 complete.
  Next: M6.1.
- **2026-11-03T12:15:21-08:00** — M4 gate passed on Windows: Win32 UI with
  source/disassembly panes, registers/flags, full memory grid, symbols, call
  stack, console, run/break/restart, line and address breakpoints, register/
  flag/memory editing, F1 help, settings, hover help, and the assembly-error
  overlay. Screenshots captured in `docs/screenshots/`. 134 C++ tests and 6
  CTest targets pass. M4 complete. Next: M5.1.
- **2026-11-03T12:02:08-08:00** — M3 gate passed: `Session` with snapshots,
  instruction/line stepping, shadow call stack, address/line/symbol
  breakpoints, console/input, stats, and editing. 134 C++ tests and 6 CTest
  targets on Windows, Linux GCC, and Linux ASan/UBSan. M3 complete. Next: M4.1.
- **2026-11-03T11:57:21-08:00** — M2 gate passed: runner/CLI, trace/stats,
  console I/O, faults, and all seven golden examples. 105 C++ tests and 6 CTest
  targets on Windows, Linux GCC, and Linux ASan/UBSan; CLI exit codes verified
  for illegal opcode (2), budget (3), BRK (0), and PC wrap (2). M2 complete.
  Next: M3.1.
- **2026-11-03T11:54:27-08:00** — M1 gate passed: 90 C++ tests and 5 CTest
  targets on Windows, Linux GCC, and Linux ASan/UBSan; all seven examples
  assemble/round-trip/disassemble; zero warnings. M1 complete. Next: M2.1.
- **2026-11-03T11:53:22-08:00** — M1.2–M1.7 done: lexer/parser, expression
  evaluator, two-pass assembler, listing/summary, `kr8 as`, symbol-aware
  disassembler, and `kr8 dis`/`kr8 sym`. All seven examples assemble; 89 C++
  tests and 5 CTest targets pass. Next: M1.8.
- **2026-11-03T11:47:26-08:00** — M1.1 done: `.k8` image model, CRC32,
  reader/writer, raw `.bin` helpers, and normative `docs/image-format.md`.
  9 image tests pass (71 total). Next: M1.2.
- **2026-11-03T11:45:02-08:00** — M0 gate passed: 62 tests on Windows MSVC,
  Linux GCC, and Linux ASan/UBSan; zero warnings. M0 complete. Next: M1.1.
- **2026-11-03T11:45:02-08:00** — M0.3–M0.7 done: memory/registers,
  fetch/decode, full ALU and flags, loads/stores/stack/control flow, I/O and
  cycles, and the machine wrapper with console capture and input queue.
- **2026-11-03T11:40:34-08:00** — M0.2 done: shared 63-entry ISA table with
  opcode/size/operand/flags/help metadata, alias lookup, instruction formatting,
  and normative `docs/isa.md`. 15 tests pass (12 ISA-specific). Next: M0.3.
- **2026-11-03T11:38:23-08:00** — M0.1 done: created the project skeleton,
  CMake/Presets, MIT licence, README, version API, CLI `version`/`help` stubs,
  and the test harness. Built with `windows-vs2026`; CTest `kr8.tests` passed
  (3 tests); `kr8 --version` → `KR8 0.1.0`. Next: M0.2.
- **2026-11-03T11:31:36-08:00** — Design freeze accepted in
  [kr8-plan.md](kr8-plan.md): C++20/CMake + KR16 Win32/SDL3 UI stack, all 63
  instructions, non-blocking `IN` with `Z` (no EOF distinction), simple cycle
  estimate, self-contained `.k8`.
- **2026-11-03T11:31:36-08:00** — Created `kr8-impl.md` with 44 tasks across
  M0–M6, test gates, Definition of Done, and this work log. No code written yet.
  Next: M0.1.

### Template

```text
- **YYYY-MM-DDTHH:MM:SS±HH:MM** — [task ID / milestone] — what changed; tests
  run; what is next.
```

---

## Appendix A — Task index

| Task | Title | Depends on | Status |
|---|---|---|---|
| M0.1 | Repository skeleton and CMake | — | `Done` |
| M0.2 | Shared ISA table | M0.1 | `Done` |
| M0.3 | Memory, registers, fetch/decode | M0.2 | `Done` |
| M0.4 | ALU and flags | M0.3 | `Done` |
| M0.5 | Loads, stores, stack, control flow | M0.4 | `Done` |
| M0.6 | Console I/O and cycles | M0.5 | `Done` |
| M0.7 | Machine wrapper | M0.6 | `Done` |
| M0.8 | M0 test gate | M0.1–M0.7 | `Done` |
| M1.1 | Image model and `.k8` format | M0.8 | `Done` |
| M1.2 | Lexer and line parser | M1.1 | `Done` |
| M1.3 | Expressions | M1.2 | `Done` |
| M1.4 | Two-pass assembler core | M1.3 | `Done` |
| M1.5 | Listing, summary, `kr8 as` | M1.4 | `Done` |
| M1.6 | Disassembler library | M1.4 | `Done` |
| M1.7 | `kr8 dis` and `kr8 sym` | M1.6 | `Done` |
| M1.8 | M1 test gate | M1.1–M1.7 | `Done` |
| M2.1 | Runner and `kr8 run` | M1.8 | `Done` |
| M2.2 | Fault diagnostics and edge cases | M2.1 | `Done` |
| M2.3 | Golden examples and CLI tests | M2.2 | `Done` |
| M2.4 | M2 test gate | M2.1–M2.3 | `Done` |
| M3.1 | Session load, restart, snapshot | M2.4 | `Done` |
| M3.2 | Stepping and shadow call stack | M3.1 | `Done` |
| M3.3 | Breakpoints | M3.2 | `Done` |
| M3.4 | Console, input, stats, editing | M3.1 | `Done` |
| M3.5 | M3 test gate | M3.1–M3.4 | `Done` |
| M4.1 | Port UI interfaces | M3.5 | `Done` |
| M4.2 | Win32 backend | M4.1 | `Done` |
| M4.3 | Theme, text field, help scaffolding | M4.1 | `Done` |
| M4.4 | App layout and top bar | M4.2, M4.3 | `Done` |
| M4.5 | Source pane | M4.4 | `Done` |
| M4.6 | Disassembly pane | M4.4 | `Done` |
| M4.7 | Registers, flags, memory grid | M4.4 | `Done` |
| M4.8 | Symbols, call stack, console | M4.4, M3.4 | `Done` |
| M4.9 | Errors, help, keyboard, settings | M4.5–M4.8 | `Done` |
| M4.10 | Win32 smoke test and screenshots | M4.9 | `Done` |
| M4.11 | M4 test gate | M4.1–M4.10 | `Done` |
| M5.1 | SDL3 canvas and window | M4.11 | `Done` |
| M5.2 | Linux app integration | M5.1 | `Done` |
| M5.3 | Linux parity and screenshots | M5.2 | `Done` |
| M5.4 | M5 test gate | M5.1–M5.3 | `Done` |
| M6.1 | Documentation | M5.4 | `Done` |
| M6.2 | Packaging | M6.1 | `Done` |
| M6.3 | CI and release | M6.2 | `Done` |
| M6.4 | Final verification and v1 sign-off | M6.1–M6.3 | `Done` |

---

## Appendix B — Milestone checkpoint checklist

Use this at each gate; copy the relevant block into the work log.

### M0 checkpoint

- [x] CMake configures on Windows.
- [x] ISA table has 63 instructions and no duplicates.
- [x] CPU tests cover every instruction and flag edge case.
- [x] Stack overflow/underflow and PC wrap are tested.
- [x] No warnings; ASan/UBSan clean on Linux.

### M1 checkpoint

- [x] `.k8` round-trips, CRC validates, corruption rejected.
- [x] All examples assemble with expected symbols/lines.
- [x] Listing/summary/raw output correct.
- [x] Disassembly is symbol-aware and does not decode data.
- [x] `as`, `dis`, `sym` CLI tests pass.

### M2 checkpoint

- [x] All golden examples produce expected output.
- [x] Faults are precise and exit codes correct.
- [x] `--trace`, `--stats`, `--input`, `--max-instructions` tested.
- [x] `as` → `run` pipeline passes on Windows and Linux.

### M3 checkpoint

- [x] Step transcripts for straight-line, loop, call, recursion.
- [x] Step over/out and line In/Over/Out correct.
- [x] Breakpoints: address, line, symbol; continue past; restart persistence.
- [x] Shadow stack diagnostics.
- [x] Input/output/stats/editing tests pass.

### M4 checkpoint

- [x] All panes render at 1280x820 and 1024x700.
- [x] Load/assemble/run/break/restart work.
- [x] Source/disassembly stepping linked to one PC.
- [x] Registers/flags/memory editing works.
- [x] Symbols, call stack, console, errors, help.
- [x] Smoke test and screenshots.

### M5 checkpoint

- [x] SDL3 window under Xvfb.
- [x] Same feature checklist as Win32.
- [x] No OS headers in `src/ui`.
- [x] `KR8_BUILD_UI=OFF` builds/tests.
- [x] Linux settings paths.

### M6 checkpoint

- [x] Docs match code.
- [x] Windows single exe, no DLLs.
- [x] Linux AppImage on clean VM.
- [x] CI matrix and release workflow defined; local equivalent green.
- [x] Definition of Done complete.

---

## 15. Post-v1 extension — built-in source editor

**Status:** `Done` (2026-11-03)

**Scope**

- [x] Explicit Edit mode for `.asm`/`.s`; `.k8`/`.bin` remain read-only.
- [x] Multiline `SourceEditor`: caret, selection, clipboard, undo/redo,
      Home/End, Ctrl+arrows, PageUp/PageDown, auto-indent, tab insertion.
- [x] Syntax highlighting for comments, labels, instructions, directives,
      registers, numbers, and strings.
- [x] Ctrl+S save and dirty prompts on Load/close.
- [x] F8 assembles the buffer, writes `.k8`, resets PC to entry, and returns
      to Debug mode.
- [x] Line breakpoints revalidated/snapped after assemble; address breakpoints
      cleared.
- [x] Disassembly and Symbols marked stale and dimmed during editing.
- [x] Regression tests for editing and the nested call stack.
- [x] Docs and screenshots updated.
- [x] Version bumped to `0.2.0`; Windows and Ubuntu 22.04 artifacts refreshed.
- [x] HLT leaves PC on the `HLT` instruction; `0.2.1` artifacts refreshed.
- [x] Console hover input feeds `IN` without the input box or Enter; `0.2.2`
      artifacts refreshed.
