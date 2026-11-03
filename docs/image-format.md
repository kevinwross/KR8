# KR8 Image Format (`.k8`)

Status: normative for image format 1.0  
Implementation: `include/kr8/image.hpp`, `src/image/image.cpp`  
Design: [../kr8-plan.md](../kr8-plan.md) §5

`.k8` is a self-contained image for the KR8 8-bit machine. It carries the
256-byte RAM image, a code map, symbols, source-line rows, and embedded source
text. All multi-byte integers are little-endian.

A raw `.bin` is only the 256-byte RAM array; it has no metadata.

## 1. Layout

```text
+--------------------------------------+
| Header (32 bytes)                    |
+--------------------------------------+
| RAM (256 bytes)                      |
+--------------------------------------+
| Code map (32 bytes, 256 bits)        |
+--------------------------------------+
| Symbols (symbol_count * 12 bytes)    |
+--------------------------------------+
| Line rows (line_count * 4 bytes)     |
+--------------------------------------+
| Source records (source_count * 12)   |
+--------------------------------------+
| String table (u32 size + bytes)      |
+--------------------------------------+
| Source blob (u32 size + bytes)       |
+--------------------------------------+
```

## 2. Header

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | `magic` | `"KR8\0"` |
| 4 | 2 | `version` | `0x0100` for format 1.0 |
| 6 | 2 | `flags` | bit 0 source, 1 symbols, 2 lines, 3 code map |
| 8 | 1 | `entry` | Initial `PC` |
| 9 | 1 | reserved | 0 |
| 10 | 2 | `image_high` | High-water mark of emitted bytes, `0..256` |
| 12 | 2 | `symbol_count` | Number of symbol records |
| 14 | 2 | `line_count` | Number of line rows |
| 16 | 2 | `source_count` | Number of source records |
| 18 | 2 | reserved | 0 |
| 20 | 4 | `crc32` | CRC32 of everything after the header |
| 24 | 8 | reserved | 0 |

`image_high` is the stack floor used by the VM/debugger. It is a high-water
mark, not a per-byte allocation map: a `.org` gap is zero-filled and is not
available to the stack.

## 3. RAM and code map

- `ram` is exactly 256 bytes. The `.bin` export is this array.
- `code_map` is 32 bytes = 256 bits. Address `a` is bit `a & 7` of
  `code_map[a >> 3]`, with bit 0 the least significant bit.
- A set bit means address `a` is the first byte of an instruction. Operand bytes
  and data bytes are clear.

## 4. Symbol record (12 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | `name_offset` into the string table |
| 2 | 2 | reserved |
| 4 | 4 | `value` (`int32`) |
| 8 | 1 | `kind` (0 = code, 1 = data, 2 = const) |
| 9 | 1 | padding |
| 10 | 2 | `size` (data size; 0 for code/const) |

`value` is an address for code/data symbols and an arbitrary value for
constants. `size` is meaningful only for data.

## 5. Line row (4 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | `address` (instruction start) |
| 1 | 1 | `file` (source index) |
| 2 | 2 | `line` (1-based) |

There is one row per instruction. A source line with no instruction has no row.

## 6. Source record (12 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | `path_offset` into the string table |
| 2 | 2 | reserved |
| 4 | 4 | `text_offset` into the source blob |
| 8 | 4 | `text_length` |

Source text is UTF-8 with LF newlines. Because the text is embedded, the
debugger does not need the original file. The path is informational.

## 7. String table and source blob

- The string table begins with a `u32` byte size, followed by NUL-terminated
  UTF-8 strings. Symbol names and source paths are offsets into this table.
- The source blob begins with a `u32` byte size, followed by raw source bytes.
  Source records give offsets and lengths into this blob.

## 8. CRC32

The CRC covers every byte after the 32-byte header. The standard reflected
polynomial is `0xEDB88320`, initial value `0xFFFFFFFF`, final inversion. A
reader rejects a mismatch with `image checksum mismatch`.

## 9. Validation rules

A reader rejects:

- a file shorter than 32 bytes;
- a bad magic or unsupported version;
- `image_high > 256`;
- a payload shorter than the fixed arrays plus the two size fields;
- a CRC mismatch;
- a truncated symbol, line, source, string table, or source blob;
- an invalid symbol kind;
- a name/path offset outside the string table or missing its NUL;
- source text outside the source blob;
- a line row whose `file` is outside the source table when sources exist.

Errors are returned in `ImageLoadResult::error`; the reader does not crash on
truncated or corrupt input.

## 10. Raw `.bin`

- `.bin` is exactly the 256-byte RAM image.
- A short raw file is zero-filled to 256 bytes; `image_high` is the file size.
- A raw file longer than 256 bytes is an error.
- Raw images have no symbols, line rows, code map, or source. The disassembler
  uses linear decoding in raw mode.

## 11. Versioning

Format version 1.0 is `0x0100`. A breaking layout change increments the major
version; additive reserved fields do not. The tool version (`0.1.0`) is
independent of the image format.
