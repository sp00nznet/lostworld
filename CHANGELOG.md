# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- First bring-up of *The Lost World: Jurassic Park* on
  [model3recomp](https://github.com/sp00nznet/model3recomp).
- `tools/build_roms.py` — assembles the program CROM, banked CROM and VROM
  from a romset, verifying the program layout against the exception vectors.
- `tools/lift.py` — boots the machine in the interpreter, snapshots RAM, and
  recompiles both halves of the program: the boot code that runs from ROM and
  the game that runs from RAM.
- `src/main.c` — loads the ROM images, registers both lifted halves, and
  enters at the reset vector.
- `docs/technical/bring-up.md` — what this title needed, and why.

### Fixed
- The game draws its own text. The tilemap decode in `model3recomp` was wrong
  in three places at once and still produced a picture; it now renders the
  region warning screen and the boot report legibly, and they match the
  strings in the ROM word for word.
- The recompiled binary no longer wedges around field 850. It was waiting at
  `0x0011837C` on a RAM word that only an interrupt handler writes, in a loop
  that touches no device and dispatches through no pointer -- so the runtime's
  field clock stopped and the interrupt could never arrive. Fixed in
  `model3recomp` by giving the runtime a turn on backward branches.
- The game reaches its main entry. Three board-level faults were in the way,
  each hiding the next: PCI configuration space did not exist, so the game
  never found the Real3D or the 53C810 and spun on the readiness flag at RAM
  `0x6FB`; the Real3D status register at `0x84000000` was a constant, so the
  frame ping the boot waits on could never change; and the interpreter
  performed SCRIPTS memory moves that `src/scsi.c` already rejected as
  out of range, one of which zeroed the boot console's text buffer pointer and
  so turned the warning screen's teardown into a `memset` over the init chain.
- The sound board never reported itself ready. The command loop at
  `0x00118854` waits on bit `0x01000000` of `0xF0080004` and runs inside the
  VBlank handler, so a board that is never ready did not merely lose sound --
  it wedged the machine, because no further field could fall due while a
  dispatch was in progress.

### Known issues
- The game submits no 3D geometry. Culling RAM holds a viewport node and an
  LOD table; the node area is 98.7% one repeated constant and polygon RAM is
  never written. Its own state machine is stalled: the frame task leaves early
  every field. Until that changes there is nothing for a Real3D renderer to
  draw. The decrementer, fixed in `model3recomp`, was one cause and is not
  the last one: the counter it drives now advances and the game still does
  not submit a scene.

### Status
- Boots, completes I/O init, takes VBlank interrupts, runs SCSI DMA, writes
  tilemap VRAM, renders it, clears the Sega region warning screen, reaches the
  main entry at RAM `0x30`, and installs and runs the frame task `0x1578` every
  field -- 497 times through field 874 in the interpreter, 496 natively.
  Attract mode is still not drawn: it is mostly 3D and the Real3D renderer is
  not written.
