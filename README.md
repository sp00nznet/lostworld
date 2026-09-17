# lostworld

**The Lost World: Jurassic Park** (Sega, 1997) statically recompiled — the
game's PowerPC code becomes native C, linked against a Model 3 board.

The board is [model3recomp](https://github.com/sp00nznet/model3recomp), carried
here as a submodule. That repository is title-agnostic: nothing in it knows
what game it is running. This repository is the bring-up for one game — the
glue, the pipeline, and the notes on what this particular title needs.

That split is the house style across the sp00nznet recomp projects: one core
toolkit per board, one repository per game. `virtuacop` and `daytonausa` sit
the same way on top of `model2recomp`.

## Status

**Alpha. The game boots and runs as native code. Attract mode is not reached.**

| | |
|---|---|
| Boots from the reset vector | yes |
| Copies itself into RAM and runs from there | yes |
| Completes I/O board init | yes |
| Takes VBlank interrupts | yes |
| Runs SCSI DMA to the Real3D | yes |
| Writes tilemap VRAM | yes |
| Renders those tiles to a framebuffer | yes |
| Agrees with the interpreter, access for access | yes |
| **Attract mode** | **not yet** |

The 2D tilemap pipeline runs end to end — ROM to lifted C to native execution
to VRAM to pixels.

The lift itself is in good shape, and the evidence is differential rather than
anecdotal: run the same boot through `model3recomp`'s interpreter and through
the recompiled binary, and the two device-access traces are identical for
30,000 accesses, with byte-identical buffers at the end — 24,525 words of VRAM
and 8,306 words of culling RAM.

### Where it stops

The game settles into its **operator service menu**, not attract mode.

Per-frame work on this title is dispatched through ten callback slots at RAM
`0x001EED7C..0x001EEDA0`. Slot `0x001EED80` should receive the frame task
`0x1578`; here it holds the null stub `0x00117864` for every field observed.
The install site is `0x000019A8`, guarded by `[0x001A3474] == 0` at
`0x00001934`. Execution provably reaches `0x000018FC` and the guard byte reads
0, so the stall sits in the three calls between — `0x0002E9BC`, the epilogue of
`0x000018BC`, and `0x00001984`.

Next step: `python ext/model3recomp/tools/ppc_interp.py ... --break-at` on each
of those, to name the last one reached.

Not the cause, though each was suspected and tested: the sound board, PCI, the
SCSI engine, the Real3D ready bit, either input polarity, or the two interrupt
lines the guest enables but the runtime never asserts.

The Real3D renderer is also unwritten, so even once attract mode starts, most
of it will be missing. That is board-level work and lives in `model3recomp`.

No screenshots of the game yet. There will be some the moment there is
something to show, and not before.

## What you need

- **model3recomp's** prerequisites: CMake 3.20+, a C17 compiler, Python 3.9+,
  optionally SDL2.
- **A Lost World romset you dumped or own.** None is included here, and none
  ever will be. Neither is any recompiled output — that is generated on your
  machine from your dump and is gitignored.

## Getting started

```
git clone --recurse-submodules https://github.com/sp00nznet/lostworld.git
cd lostworld
```

### 1. Build the ROM images

```
python tools/build_roms.py /path/to/lostwsga.zip roms/
```

This works out which chips are the program ROMs by trying interleaves and
checking the result against the PowerPC exception vector table, so it proves
the layout rather than assuming it:

```
program EPROMs : epr-19936.20, epr-19937.19, epr-19938.18, epr-19939.17
maps at        : 0xFFE00000
reset vector   : 0xFFF00100
verified       : 7/7 exception vectors
banked CROM    : roms/lw_bank.bin  (0x2000000 bytes from 16 chips)
VROM           : roms/lw_vrom.bin  (0x4000000 bytes from 16 chips)
```

### 2. Recompile the game

```
python tools/lift.py
```

This boots the machine in an interpreter first, because Model 3 games do not
run from ROM — see [docs/technical/bring-up.md](docs/technical/bring-up.md).
It writes `src/recomp/`, which is **not** committed.

### 3. Build and run

```
cmake -S . -B build
cmake --build build --config Release
./build/lostworld
```

Pass a field count to capture a screenshot and exit, which is what the
conformance runs do:

```
./build/lostworld 600      # writes shot.ppm after 600 fields
```

## When it stops making progress

A recompiled game has no program counter to inspect, so the runtime reports
what it can:

| Symptom | What it means |
|---|---|
| `no function lifted at XXXXXXXX` | an indirect target the lifter missed — collect them and re-run `tools/lift.py --entries` |
| `unimplemented <mn> at XXXXXXXX` | an instruction the lifter does not translate |
| `N device accesses with no field advance` | the guest is polling one register forever; the report names it |
| nothing at all, frames ticking | the guest fell out of lifted code — check `M3_TRACE_CALLS=40` |

## License

MIT — see [LICENSE](LICENSE).

No ROMs, dumps, recompiled source, symbol maps or anything else derived from
the game binary is in this repository, and none ever will be. The tools ship;
the output does not.
