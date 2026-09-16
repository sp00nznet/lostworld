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

### Status
- Boots, completes I/O init, takes VBlank interrupts, runs SCSI DMA, writes
  tilemap VRAM, and renders it. Attract mode is not reached: it is mostly 3D
  and the Real3D renderer is not written.
