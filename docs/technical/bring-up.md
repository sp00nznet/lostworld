# Bring-up notes

What *The Lost World* specifically needed. Board-level behaviour lives in
[model3recomp's execution-model doc](../../ext/model3recomp/docs/technical/execution-model.md);
this file is the title's own story and the addresses are its.

## The ROM

Four program EPROMs interleave as byte-swapped 16-bit words into one 2 MB
image that occupies the top of the address space, `0xFFE00000`–`0xFFFFFFFF`:

| EPROM | byte lane |
|---|---|
| `epr-19936.20` | 0–1 |
| `epr-19937.19` | 2–3 |
| `epr-19938.18` | 4–5 |
| `epr-19939.17` | 6–7 |

`tools/build_roms.py` does not take that on trust. It tries interleaves and
keeps the one where each PowerPC exception vector opens by loading its own
number (`li r7, n` at `0xFFF00000 + 0x100n`), which only the correct layout
produces.

The remaining chips: sixteen 2 MB parts form the banked CROM at `0xFF000000`,
sixteen 4 MB parts form the VROM, and the two 4 MB parts that sort after them
are the SCSP sample ROMs.

## The boot

1. `0xFFF00100` calls a table-driven hardware initialiser at `0xFFF00508`,
   which programs the MPC105 host bridge at `0xF8FFF000`.
2. `0xFFF00354` walks a table of `(src, dst, count)` triples, copying the game
   out of CROM into RAM:

   | RAM | from CROM | size |
   |---|---|---|
   | `0x000000..0x0FFFFC` | `0x000000` | 1 MB, verbatim |
   | `0x100000..0x13B2D4` | `0x120004` | 237 KB |

3. `0xFFF00150` sets LR to zero and executes `blr` — a jump to RAM
   `0x00000000`, not a return. RAM `0x0` is nine consecutive `bl`
   instructions, the game's init chain.

That is why `tools/lift.py` boots the machine before it lifts it.

## What this title needs from the board

| Address | Behaviour | If wrong |
|---|---|---|
| `0xF0100008` | byte register; reads back what was written | spins on CROM bank select |
| `0xF0040000` | strobe latch; must mirror both edges | I/O init never starts |
| `0xF0040004` | serial ready line must change state | I/O init never finishes |
| `0xF1180010` | writing a bit clears it in `0xF0100018` | never leaves its interrupt handler |
| `0xC1000014` | SCSI ISTAT; DIP set by a SCRIPTS `INT` | polls ~20 M times |

The main loop never reads the interrupt controller. It calls a service routine
through a pointer and waits on a flag byte at RAM `0x6ED` that only the VBlank
handler sets:

```
0x00118284  lwz r0, -0x126C(r29) ; mtlr r0 ; blrl
0x00118290  lbz r0, 1(r30)
0x001182A0  bc  0x00118284
```

So interrupts have to arrive on a timer, which is what the board does anyway.

## Graphics

The tilemap name table is at VRAM `0x0F6000` and a name entry's pattern
address is the entry times sixteen — entry `0x0080` resolves to byte `0x800`,
where there is a glyph, while times thirty-two lands on blank. Palette entries
are 32 bits with a 1-5-5-5 colour in the first half.

Attract mode is mostly 3D, so the tilemap alone does not get there.
