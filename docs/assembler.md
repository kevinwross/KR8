# KR8 Assembler

Status: matches ISA v1 and image format 1.0  
Instruction reference: [isa.md](isa.md)  
Image format: [image-format.md](image-format.md)

`kr8 as` turns one assembly source file into a self-contained `.k8` image. There
is no linker, no object format, and no separate relocation step.

```text
kr8 as hello.asm
kr8 as hello.asm -o build/hello.k8 --raw build/hello.bin -l
```

## 1. Source form

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
- `;` starts a comment that runs to the end of the line.
- Labels are `name:` and may share a line with an instruction or directive.
- Labels and `.equ` names are case-sensitive.
- Mnemonics, directives, and register names are case-insensitive.
- Indentation and whitespace are free.
- A line may be blank or contain only a comment.

## 2. Operands

| Form | Meaning | Example |
|---|---|---|
| `A`, `B` | Register | `MOV A, B`, `INC B` |
| `#expr` | 8-bit immediate | `MOV A, #'A'`, `ADD A, #1` |
| `[expr]` | Direct memory address | `LD A, [msg]`, `ST [buf], A` |
| `[B]` | Memory at the address in B | `LD A, [B]`, `ST [B], A` |
| `expr` | Absolute branch/call target | `JMP loop`, `JZ done` |

There is no indexed `[B+disp]` and no `[PC+disp]`. With 256 bytes, direct
addresses are always available, and `INC B`/`DEC B` covers pointer walking.

## 3. Instructions

The complete 63-instruction set, encodings, flags, and cycle estimates are in
[isa.md](isa.md). The assembler follows that table exactly. Branch aliases such
as `BEQ`, `BNE`, `BCS`, `BCC`, `BMI`, `BPL`, `BVS`, `BVC`, `BLT`, `BGE`, `BGT`,
`BLE`, and `BRA` are accepted; the disassembler prints canonical `Jxx` names.

## 4. Directives

| Directive | Meaning |
|---|---|
| `.org expr` | Set the location counter to `0..255`; forward moves only |
| `.byte expr, ...` | Emit one byte per expression (`-128..255`) |
| `.word expr, ...` | Emit two bytes per expression, little-endian |
| `.ascii "text"` | Emit the bytes of `text`, no terminator |
| `.asciz "text"` | Emit the bytes of `text`, then a NUL |
| `.space expr` / `.zero expr` | Reserve `0..256` zero bytes |
| `.entry expr` | Set the image entry address |
| `.equ NAME, expr` | Define a constant; no storage |

Deferred: `.include`, `.align`, macros, local-label scoping, and conditional
assembly.

`.org` gaps are zero-filled and count toward the image only when bytes are
actually emitted. `image_high` is the high-water mark of emitted bytes and
becomes the stack floor.

## 5. Literals and expressions

- Decimal: `123`
- Hex: `0x7B` or `$7B`
- Binary: `0b01111011`
- Character: `'A'`, with escapes `\n`, `\r`, `\t`, `\0`, `\\`, `\'`, `\"`,
  `\xNN`
- Strings: `"text"` with the same escapes
- Unary: `+`, `-`, `~`
- Binary: `+`, `-`, `*`, `/`, `%`, `<<`, `>>`, `&`, `^`, `|`
- Parentheses
- Symbols and `.equ` constants

Expressions are evaluated as signed 32-bit integers with C-like precedence.
Operands are range-checked at the point of use. Division or remainder by zero is
a diagnostic.

Examples:

```asm
.equ MASK, 1 << 3
        MOV A, #MASK          ; 8
        .byte 1 + 2 * 3       ; 7
        .word 0x1234          ; 34 12
        MOV A, #'A'           ; 0x41
```

## 6. Passes and forward references

The assembler is two-pass:

1. Parse lines, determine instruction sizes from syntax, place labels, process
   `.org`/`.space`/`.equ`, and classify symbols.
2. Resolve operands, encode, fill RAM, build the code map, line rows, symbols,
   and listing.

Instruction size never depends on a symbol value. `#expr` and `[expr]` forms are
always two bytes; register and `[B]` forms are always one byte. Forward
references in instruction operands are normal. `.org` and `.space` need values
that are known during pass 1; a forward label there is an error.

Entry point:

- `.entry expr` sets it explicitly.
- Otherwise it is the first instruction emitted.
- If there are no instructions, assembly fails.
- An entry that is not an instruction produces a warning.

## 7. Symbols

| Kind | Meaning |
|---|---|
| Code | Label attached to an instruction; branch/call target |
| Data | Label attached to `.byte`/`.word`/`.ascii`/`.asciz`/`.space`; has a size |
| Const | `.equ` name and value |

All symbols are visible to the disassembler and debugger. There is no
`.global`/`.local` split because there is no linker.

## 8. Output and summary

By default `kr8 as hello.asm` writes `hello.k8` next to the source. `-o` changes
the path. `--raw` also writes the raw 256-byte RAM. `--no-source` omits the
embedded source text.

The summary reports:

```text
hello.k8: 10 code bytes, 13 data bytes, image_high=23, stack headroom=233
```

The stack grows down from `0xFF`; the assembler warns when headroom is below 16
bytes.

## 9. Listing

`-l`/`--listing` prints:

```text
ADDR  BYTES     SOURCE
0000  0B 0A     MOV B, #msg
0002  12        LD A, [B]
...
Symbols:
  0002 code  loop
  0009 code  done
  000A data  msg (13 bytes)

Image: 10 code bytes, 13 data bytes, image_high=23, stack headroom=233
```

## 10. Diagnostics

```text
hello.asm:12:9: error: undefined symbol 'done'
```

Errors are collected; no output file is written if any error occurs. Warnings
do not fail assembly. Common errors:

- unknown mnemonic or directive
- invalid operand form or count
- undefined or duplicate symbol
- `.org` backwards or outside `0..255`
- `.space` outside `0..256`
- immediate/address/target out of range
- two-byte instruction starting at `0xFF`
- image exceeding 256 bytes
- division by zero
- unterminated string or invalid escape
- label with no following code or data

The UI shows the diagnostics in an overlay; clicking a line jumps to the source
line. The previous image stays loaded and step-able after a failed assembly.

## 11. CLI reference

```text
kr8 as <file.asm> [-o out.k8] [--raw out.bin] [-l] [--no-source] [-q]
```

- `-o <path>` — output image path.
- `--raw <path>` — also write the 256-byte binary.
- `-l`, `--listing` — print the listing.
- `--no-source` — do not embed the source text.
- `-q`, `--quiet` — suppress the summary.

Exit code 0 on success, 1 on assembly or output failure, 2 on usage errors.
