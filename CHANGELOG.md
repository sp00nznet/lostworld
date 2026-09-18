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
- The game reaches its main entry. Three board-level faults were in the way,
  each hiding the next: PCI configuration space did not exist, so the game
  never found the Real3D or the 53C810 and spun on the readiness flag at RAM
  `0x6FB`; the Real3D status register at `0x84000000` was a constant, so the
  frame ping the boot waits on could never change; and the interpreter
  performed SCRIPTS memory moves that `src/scsi.c` already rejected as
  out of range, one of which zeroed the boot console's text buffer pointer and
  so turned the warning screen's teardown into a `memset` over the init chain.

### Status
- Boots, completes I/O init, takes VBlank interrupts, runs SCSI DMA, writes
  tilemap VRAM, renders it, clears the Sega region warning screen, reaches the
  main entry at RAM `0x30`, and installs and runs the frame task `0x1578`.
  Attract mode is still not drawn: it is mostly 3D and the Real3D renderer is
  not written.
