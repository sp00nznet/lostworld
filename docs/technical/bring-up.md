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
| `0xF0C00CFC` | PCI config: device 13 is `0x16C311DB` | never leaves `0x001179C8` |
| `0xF0C00CFC` | PCI config: device 14 is `0x00011000` | SCSI never configured |
| `0x84000000` | bit 1 must change between reads | never leaves `0x00117A28` |

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

## Getting to the main entry

RAM `0x0` is nine `bl` instructions, and then two that matter:

```
0x0000002C  bl 0x0011AF60      ; the kernel's own start-up
0x00000030  b  0x00001A34      ; tail jump to main -- not a call
0x00000034  b  0x00000034      ; "cannot get here"
```

`0x30` is a branch, not a call, so `0x0011AF60` returning is the only thing
that starts the game. It is a long way down: `0x0011B068` calls `0x0010F564`,
which calls `0x001179C8`, which is where the Real3D handshake lives. Each of
those had a wait in it that the board was not answering -- see the README for
the three that were wrong.

The installer itself is one function beginning at `0x00001934`. There is no
epilogue between `0x00001980` and `0x00001984`, so the `[0x001A3474]` guard at
`0x00001984` and the install site at `0x000019A8` belong to it:

```
0x00001984  lwz   r0, [0x001A3474]
0x00001994  bc    -> 0x000019B0      ; non-zero picks the other task
0x000019A4  addi  r3, r9, 0x1578     ; the frame task
0x000019A8  bl    0x00117C44         ; into slot 0x001EED80
```

The boot takes about 370 fields to get here, most of it the Sega region
warning screen, which is displayed deliberately and times out after 299.

## History: from boot to attract mode

What follows was the README's status section while the game was being
brought up, kept as it was written. It is a record of the stalls and how each
was found, not a description of the current state -- by the end of it the
game was not yet drawing anything in 3D, and it now plays.

### What was blocking it

The game used to settle into its **operator service menu** instead of starting.
Per-frame work is dispatched through ten callback slots at RAM
`0x001EED7C..0x001EEDA0`, and slot `0x001EED80` never received the frame task
`0x1578` -- it held the null stub `0x00117864` for every field observed.

Three board-level faults were in the way, each hiding the next. All three are
fixed; the notes below are what they were, because none of them announced
itself and the first two had been wrongly ruled out.

**1. PCI configuration space did not exist.** The game will not come up until
it finds two devices by ID, through the MPC105's port pair at `0xF0800CF8`
and `0xF0C00CFC`:

| device | must answer | |
|---|---|---|
| 13 | `0x16C311DB` | Sega Real3D |
| 14 | `0x00011000` | LSI 53C810 |

The device-13 arm is what sets the readiness flag at RAM `0x6FB`, and
`0x001179C8` spins on that flag forever without it. Both devices already live
at fixed addresses here, so the BAR writes that follow are dropped.

**2. The Real3D status register was a constant.** The boot copies nine dwords
from `0x84000000` into RAM with `stwbrx` -- the chip is on the PCI side, so
the cached copy is byte-reversed -- and then waits for bit `0x02000000` of
that copy to flip. Byte-reversed, that is bit 1 of the register. A constant
never flips. It is toggled per read now, which is all the wait needs.

**3. A malformed SCRIPTS program was zeroing work RAM.** `src/scsi.c` already
refused memory moves whose source or destination was not mapped;
`tools/ppc_interp.py` did not, and performed them. A bulk write through the
`0xC0000000` window kicks SCRIPTS with garbage DSP values -- the register
image is written a word at a time and one word lands on offset `0x2C` -- and
one of the resulting moves, `2C022031 -> 00000980, 8064 bytes`, zeroed RAM
`0x980..0x2900`. That range holds `[0x00000E5C]`, the boot console's text
buffer pointer. Three hundred fields later the Sega region warning screen
tore itself down with `memset([0x00000E5C], 0, 8KB)`, and with the pointer
zeroed that wiped RAM `0x0..0x2000` -- including the init chain at `0x30`.

With those three fixed the boot runs to completion:

| | field |
|---|---|
| RAM `0x30`, the tail jump to main | 369 |
| `0x00001934`, the installer | 369 |
| `0x00001984`, the `[0x001A3474] == 0` guard | 377 |
| `0x000019A8`, the install site | 377 |
| `0x00001578`, the frame task running | 378 |

The 299-field pause in the middle is not a fault: it is the Sega region
**WARNING** screen, displayed for about five seconds by design.

`tools/ppc_interp.py --break-at ADDR` reports whether execution ever reaches
an address, and is repeatable, so one run answers a whole call chain. That is
how each of these was found.

### Where it stops now

The frame task runs every field, the interrupt path is healthy, and the game
draws its text. Then it stops making progress, and the reason is not the one
it first appears to be.

The most recent thing found and fixed was the **decrementer**. The game's
timing calibration waits on a counter at RAM `0x001C10D0`, and the only code
that writes it is the handler the game installs at exception vector `0x900` —
a pointer that appears nowhere in RAM, because the processor calls it rather
than the game. `model3recomp` had no decrementer, so that wait could never
end; the interpreter and the recompiled binary sat in it forever and agreed
exactly, which is what ruled out a lifter bug. With SPR 22 counting down and
its `0 -> -1` crossing taking the exception, the counter advances.

It still does not reach attract mode, and the reason is now narrow enough to
state exactly. Instrument every guest function entry rather than only the
indirect dispatches and the frame is legible:

| | |
|---|---|
| Frame task `0x1578` | runs every field |
| Real3D triggered | ~476 times per 2,500 fields, by DMA of a 4-byte word |
| Culling RAM written | 23 small transfers, all in the first second |
| Polygon RAM written | never |
| Upload FIFO at `0x94000000` | 464 transfers — a gamma ramp, re-sent every frame |
| Tilemap | deliberately blank after the boot report |

So the guest is not stalled in the sense of being stuck in a loop: it runs a
frame, sets its colour ramp, triggers the renderer, and does it again. It just
never sends any geometry. The blank screen after the boot report is the game's
own doing, not a renderer that cannot draw.

One real fault was found along the way and fixed: the FIFO above is a single
port rather than an address window, and the DMA range check had been rejecting
every transfer to it, because a thousand bytes to one address looks like a
thousand bytes off the end of it.

### The nearest thing to a cause

Tracing the code that *would* submit geometry gets to something specific.
Polygon RAM is written from exactly one place, `0x0010FC14`, which stages a
scene at RAM `0x001BAA50` and DMAs it to `0x98001000`. That is called only
from `0x00001A64`, which has four callers — `0x000022CC`, `0x000298A4`,
`0x00029D64`, `0x000A70DC` — and **none of the four ever runs**, so the whole
path is dormant.

Upstream of that, the game's own OS dispatches a callback slot only when the
matching bit is set in the flags word at RAM `0x000007F4`:

```
001183C4  lwz    r11, [0x000007F4]
001183C8  andis. r0, r11, 0x1000       ; bit 0x10000000
001183D8  bc     -> 0x001183FC         ; clear: skip the task entirely
001183E0  lwz    r9, [0x001EED98]      ; else call what is in that slot
001183F4  blrl
```

Slot `0x001EED98` holds a real task, `0x00032B2C`. Its bit is never set —
`[0x000007F4]` reads `0x20000800` from the first field to the ten-thousandth
— so that task has never once run. The OS sets these bits at `0x00116E04`.

Forcing the bit is not the fix and was tried: the task still does not start
and the scheduler comes apart, which says the task has to be *started*
properly rather than merely marked. Forcing it once rather than every field
does work, and draws the operator test menu — so the dispatch mechanism and
the tilemap are both fine, and that slot holds the service menu rather than
anything to do with attract mode.

### The ROM images were built wrong

`mame -listxml lostwsga` gives the authoritative layout, region by region and
offset by offset, and `model3recomp`'s loader disagreed with it in a way that
size checks cannot catch. The obvious reading is the wrong one:

| | was | is |
|---|---|---|
| banked CROM | sixteen **2 MB** parts, 32 MB | sixteen **4 MB** parts, **64 MB** |
| VROM | sixteen **4 MB** parts, 16-lane | sixteen **2 MB** parts, **8-lane** |
| group order | chip-number order | reversed (CROM), pairs swapped (VROM) |

Both images came out the right size either way, so nothing complained, and
the game read plausible nonsense rather than nothing — which is worse, because
it gets further before going wrong. The corrected banked CROM is verified
chip by chip against MAME's layout: each part lands exactly where that layout
puts it, byte-swapped, eight bytes apart.

It also settles something the guest had been saying all along. 64 MB in an
8 MB window is **eight** banks, not four — which is why it writes `F0`
through `F7`, values no four-bank reading could account for.

### Six megabytes of the data ROM were never mapped

The one measured with MAME rather than reasoned about. Run the game under
MAME, read its memory back, and the CROM map is plain:

| CPU address | what is there |
|---|---|
| `0xFF000000`–`0xFF7FFFFF` | zeros — on real hardware too |
| `0xFF800000`–`0xFFDFFFFF` | the data image, offset 0 onward, **unbanked** |
| `0xFFE00000`–`0xFFFFFFFF` | the 2 MB program |

`model3recomp` had nothing at all in the middle span, so a game asking for its
own tables got zeros. This one reads `0xFFA18DEC`, where the image holds an
`"M3"`-tagged record, and found none of it. Now mapped, and checked address by
address against MAME: `0xFF800000` is offset 0, `0xFFC00000` is `0x400000`,
`0xFFDFFFF0` is `0x5FFFF0`.

It also closes the bank-register question: driving that register through all
256 values under MAME moves none of this. The span is not banked, so no
reading of those bits was ever going to be the answer — which is why none of
the two dozen tried worked.

### What the game should be showing, and where it stops

`mame -listxml` was useful; running MAME is better. It plays this game on this
machine, so the question "what should be on screen at this point" has an
answer rather than a guess. Two things fall straight out of that.

The first is a check on the tilemap work above: MAME's frame 300 has **3,757**
non-black pixels and its frames 600 and 900 have **838** — the same counts
this port produces, exactly. The 2D pipeline agrees with a reference
implementation pixel for pixel on both screens it can draw.

The second is where it stops. MAME's attract mode begins around frame 1,200
with a credits line at the bottom of the screen, and by frame 3,300 it is
showing a ranking table — *drawn with the tilemap*, not the Real3D. So attract
mode is partly reachable without a renderer at all.

Comparing the two machines at the same point:

| | |
|---|---|
| work RAM, in the ranges dumped | **904 of 969** non-zero words identical |
| flags word, game mode, scissor, callback slots | identical |
| the tilemap copy at `0x0002CC04` | MAME runs it repeatedly, this runs it **once** |
| its gate at RAM `0x001C1B70` | set by the game's text-drawing calls, which MAME makes and this does not |
| the credits line | MAME writes rows 43-44; this writes rows 0-1 |

So the two are in very nearly the same state, and the gap is that the attract
sequence does not drive the text API.

Following that upward: the text calls come from the game's state handlers, in
a table at RAM `0x0011E6A4` indexed by `[0x001A3BB4] & 7`. That state is `0`
in both machines, so both should be running the handler at `0x00001EB8` — and
the dispatcher that would call it is the game's own main loop:

```
00001A14  bl 0x00001EF4     ; dispatch one state
00001A18  b  0x00001A0C     ; forever
```

**That loop used to go round once here, and now it turns.** Its last call is `0x00118340`, which waits
at `0x0011837C` for RAM `0x001C10D0` to change, and only the decrementer
handler writes that word. The handler does run — 43 times in a 3,000-field
run — and the word does change, but the wait is 568 million iterations of that
one branch, by far the hottest in the program.

It was not the decrementer's rate — sweeping that over four orders of
magnitude left the iteration count identical to the digit. It was the **time
base**.

`mftb` advanced one tick per *read*, which is not a clock: its value depended
on how often the guest looked at it rather than on how much time had passed.
This game times an interval with it and divides to get its decrementer period,
and computed **−26** where the hardware gives **26,926**. So the decrementer
never fired again, the wait never ended, and the state handler ran once
instead of once a field. The other end of the same measurement was the Real3D
frame flag, which flipped per read rather than per frame — a shortcut taken
deliberately, marked "upgrade path: flip it when the Real3D actually retires a
frame", and now come due.

With both fixed the machine comes alive:

| | before | after |
|---|---|---|
| timing routine `0x00118340` | 1 | **2,059** |
| tilemap copy `0x0002CC04` | 1 | **2,059** |
| state handler `0x0001D68C` | 1 | **2,060** |

The game now runs its own state machine once a field, in state 0, the same
state MAME is in. What it still does not do is drive its text calls, so the
credits line MAME draws is not drawn here yet.

Meanwhile the frame task keeps running, because it arrives by interrupt rather
than through this loop — which is why fields advance and the screen updates at
all while the main loop is stuck. That also defeats the field-advance loop
guard, so the backward-branch histogram is what found this. That is a narrower thing to chase than
anything before it, and the method for chasing it now exists: MAME's memory
taps name the code that writes a given address, and its RAM can be diffed
against this port's word for word.

### Earlier suspicion: the banked window

The banked CROM window is 8 MB of a 32 MB image, so at most two bits of the
register at `0xF0100008` can be the bank. `model3recomp` shifted the whole
byte, and this game writes `0xF7` for most of its run — which lands at offset
`0xF700000`, far outside the image. Every read through the window then returns
zero: **299,984 of them in a 2,500-field run, about 1.2 MB of the game's own
data.** `tools/rom_loader.py` warns of precisely this in a comment, and the
symptom is what it predicts.

That is very likely why nothing is drawn: the object and model tables the
attract sequence would walk are read as zeros, so there is nothing to submit.

One fix that this needed is in and correct: the register read back a value
derived from the bank *offset*, which agrees with what was written only while
the mapping is a plain shift — so every attempt to correct the mapping wedged
the boot in the spin that waits for the readback, and looked like a different
bug. Register and offset are separate now.

The mapping itself is still unknown and is deliberately left alone rather than
guessed at. `M3_TRACE_BANK` reports the access pattern — this game reads
window offsets `0x000000`, `0x010000`, `0x120000`, `0x150000` and `0x400000`
under registers `F1`, `F2`, `F3` and `F5` — and `M3_CROM_BANK` selects between
candidate readings. Every candidate tried so far leaves the game worse off
than zeros do, which says it is finding wrong data rather than none.

**There is no 3D scene to draw, which is why the Real3D renderer is not the
next thing to write.** Dump the scene memory and the picture is unambiguous:

| | |
|---|---|
| Culling RAM, high | a viewport node and an LOD table — real, and frozen |
| Culling RAM, low | never written |
| Polygon RAM | never written |
| The node area | 98.7% one repeated constant, `0x00800800` |

So the guest has set the Real3D up — the viewport node at offset 0 holds
believable frustum plane normals as little-endian floats, `0.2588` and
`0.9659` among them — and then submits no models at all. A rasteriser written
today would walk an empty node list and draw nothing. The blocker is the
frozen counter above, not the renderer.

`M3_LOOP_GUARD` is how the last stall was found. A recompiled game has no
program counter to inspect, so a build with it defined reports the guest
address a wedged game is spinning at; that is what named `0x0011837C`, a wait
on a RAM word that only an interrupt handler writes.
