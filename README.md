# Z8001 Short Segmented Address Support

## Overview

The Z8001 CPU supports two forms for segmented addresses in Direct Address
(DA) and Index (X) addressing modes:

- **Long form** (2 words / 4 bytes): 7-bit segment + 16-bit offset
- **Short form** (1 word / 2 bytes): 7-bit segment + 8-bit offset

The short form saves 2 bytes per instruction when the offset within a
segment fits in 8 bits (0x00-0xFF). See Z8000 Technical Manual, Section 5.3.

This patch adds automatic short form support at both assembly time and link
time. No source changes are needed — the assembler and linker automatically
use short form whenever the offset fits in 8 bits.

## Changes

### Assembler (`gas/config/tc-z8k.c`, `gas/config/tc-z8k.h`)

**Short form for resolved addresses**: When assembling in Z8001 mode,
resolved absolute values (`.equ` constants) with offset <= 0xFF are
automatically emitted in short form (2 bytes). The standard segmented
address encoding (`0x82000000` for segment 2, offset 0) is used unchanged.

**`--linkrelax` flag**: When passed to the assembler, non-absolute,
non-PC-relative symbol references emit relocations instead of being resolved
at assembly time. This allows the linker to recompute addresses after
relaxation shrinks instructions. PC-relative fixups (`jr`, `calr`, `djnz`,
`dbjnz`) are still resolved by the assembler to avoid overflow on
short-range branches. Required when using `--relax` at link time.

**`.long_addr` directive**: Forces the next segmented address reference to
use long form regardless of `--relax`. The assembler emits an
`R_IMM32_NORELAX` relocation instead of `R_IMM32`, which the linker's
relaxation pass skips. Applies to one instruction only; resets after use.

**`TC_FIX_TYPE` / `tc_fix_data`**: Per-fixup integer flag used to propagate
the `.long_addr` state from `build_bytes` through to `tc_gen_reloc`, where
the reloc howto is switched to `r_imm32_norelax`.

### Linker (`bfd/coff-z8k.c`)

**`z8k_relax_section`**: Replaces the generic `bfd_coff_reloc16_relax_section`.
Scans R_IMM32 relocations in each input section. If the target's segment
offset (adjusted for prior relaxations) fits in 8 bits, the reloc is marked
as R_IMM32_SHORT. Multi-pass until convergence. After convergence, adjusts
all symbol values in the section to account for the cumulative shrinkage.
Correctly handles cross-section (cross-object) symbol references by looking
up undefined symbols in the linker hash table.

**Cross-section relaxation**: The original code skipped relocations whose
symbol had `section->flags == 0`. Since `bfd_und_section` (used for
cross-object symbols) has flags 0, this prevented all cross-section
relaxation. Fixed by replacing the flags check with a proper resolvability
test: undefined/common symbols are looked up in the linker hash table and
skipped only if they have no definition.

**`R_IMM32_SHORT` in `extra_case`**: Emits the short form segmented address
(2 bytes) while consuming 4 source bytes, creating the size reduction.

**`R_IMM32_NORELAX` in `extra_case`**: Handled identically to `R_IMM32` —
produces long-form segmented encoding. The relaxation loop ignores it since
it only matches `R_IMM32` and `R_IMM32_SHORT`.

**`src_ptr` fix in `extra_case`**: The R_IMM4L, R_DISP7, and R_CALLR
handlers read opcode bits from `data[*src_ptr]` instead of `data[*dst_ptr]`
to get the correct source byte when relaxation has shifted the src/dst
positions apart.

### COFF header (`include/coff/z8k.h`)

**`R_IMM32_NORELAX` (0x31)**: New relocation type for long-form segmented
addresses exempt from relaxation.

## Relocation types

| Type | Value | Description |
|------|-------|-------------|
| `R_IMM32` | 0x11 | 32-bit segmented address, eligible for relaxation |
| `R_IMM32_SHORT` | 0x30 | Internal: relaxed short form (linker only, not in object files) |
| `R_IMM32_NORELAX` | 0x31 | 32-bit segmented address, exempt from relaxation |

## Usage

### Assembling

    z8k-coff-as -z8001 --linkrelax -I include -o output.o source.s

- `-z8001`: Z8001 segmented mode
- `--linkrelax`: Emit relocations for all symbol references so the linker
  can recompute displacements after relaxation

Without `--linkrelax`, the assembler resolves same-section references at
assembly time. Short form works for `.equ` constants but the linker cannot
relax cross-section references.

### Linking

    z8k-coff-ld -mz8001 --relax -e entry -Ttext 0x40000 -o output.coff input.o

- `--relax`: Enable linker relaxation (converts eligible long-form label
  references to short form)

### Full pipeline

    z8k-coff-as -z8001 --linkrelax -I include -o prog.o prog.s
    z8k-coff-ld -mz8001 --relax -e _start -Ttext 0x40000 -o prog.coff prog.o
    z8k-coff-objcopy -O binary prog.coff prog.bin

### Forcing long form

Use `.long_addr` before an instruction to prevent relaxation of its address:

```asm
    ld  r0, label       ! may be relaxed to short form
    .long_addr
    ld  r1, label       ! always long form, even with --relax
    ld  r2, label       ! .long_addr has been consumed; may be relaxed
```

## How it works

The assembler handles `.equ` constants at assembly time. Any resolved
segmented address with offset <= 0xFF is emitted in short form automatically.
The standard long-form encoding (`0x82000000` = segment 2, offset 0) works
unchanged — the assembler extracts the segment from bits 30-24 and the
offset from bits 15-0, regardless of bit 31.

Label references (same-section and cross-section) are emitted as long form
by the assembler and relaxed to short form by the linker when the resolved
offset fits in 8 bits. The linker iterates until no more relaxations are
possible.

## Files modified

- `include/coff/z8k.h` — `R_IMM32_NORELAX` relocation type
- `bfd/coff-z8k.c` — linker relaxation, reloc handling
- `gas/config/tc-z8k.c` — assembler address emission, `.long_addr`, force relocation
- `gas/config/tc-z8k.h` — `TC_FORCE_RELOCATION`, `TC_FIX_TYPE`
