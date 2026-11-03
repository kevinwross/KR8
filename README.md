# KR8

KR8 is an 8-bit teaching computer: a 256-byte machine, a small assembler and
disassembler, and a windowed debugger modelled on KR16's UI.

The project is under construction. The design is frozen in
[kr8-plan.md](kr8-plan.md); the task-level checklist is
[kr8-impl.md](kr8-impl.md).

## Status

Milestone **M0** — skeleton, ISA table, CPU, and machine tests.

## Build

Requirements:

- CMake 3.25+
- C++20 compiler
  - Windows: Visual Studio 2026 or Ninja + MSVC
  - Linux: GCC 13+ or Clang 16+

Windows with Ninja + MSVC (from a developer command prompt):

```text
cmake --preset windows-ninja
cmake --build --preset windows-ninja-debug
ctest --preset windows-ninja-debug
```

Visual Studio generator:

```text
cmake --preset windows-vs2026
cmake --build --preset windows-vs2026-debug
ctest --preset windows-vs2026-debug
```

Linux:

```text
cmake --preset linux-gcc
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

## Quick start

Once the toolchain milestones land:

```text
kr8 as examples/hello.asm
kr8 run examples/hello.k8
kr8
```

`kr8` with no arguments opens the debugger when the UI is built.

## Documentation

- [kr8-plan.md](kr8-plan.md) — design contract
- [kr8-impl.md](kr8-impl.md) — implementation plan and checklists
