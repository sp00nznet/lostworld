# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Changed
- model3recomp no longer contains code derived from Supermodel (GPL); the
  parts that were have been rewritten from MAME (BSD-3-Clause) and from
  measurements of this game. Specular highlights are subtler, and save
  states made before this will not load.

### Added
- **Start at stage** (Debug menu): begin a game on any of the five stages,
  whichever is picked on the game's own INGEN STAGE SELECT screen. The stage
  is a word at RAM 0x1C2B10, the stage number minus one, held from the
  start press until the stage is under way.

- **Fog, mipmaps and specular highlights**, from model3recomp's renderer:
  the dusty stages are hazy, the T-Rex's night darkens with distance,
  distant ground and roofs no longer shimmer, and the jeeps shine.
- **The operator settings in the Debug menu**, as the cabinet manual lists
  them: difficulty (the test menu's 16-step bar), starting life (1-9),
  boss action (mild / normal), attract sound, and **free play** (coin
  setting #27). Found from the routines that draw them in the test menu,
  confirmed on its GAME ASSIGNMENTS page; they live in the settings block
  at RAM 0x121C and are applied every field. Changing one restarts the
  game; netplay uses the host's.
- Infinite health tops up to the starting life, not to three.
- **Sound**: music, voices and effects, from model3recomp's new sound board.
  `tools/build_roms.py` now also writes `lw_snd.bin` (the 68000 program)
  and `lw_samples.bin` (the wave ROM). Attract mode has sound from the
  title sequence on; the boot and the region warning are silent, as they
  are under MAME.
- **English.** A Region setting (Debug menu, USA by default; changing it restarts the game) sets the country
  byte the game picks its text by -- RAM `0x1226`, the EEPROM settings'
  copy. The Japanese board carries the American attract screens, subtitles
  and prompts; no other dump is needed.
- **The HUD**: the ammunition counter, the RELOAD prompt, pickups and the
  T-Rex's target circles, which were being painted over (model3recomp's
  viewport priority fix). A scripted run with infinite health beats the
  T-Rex and reaches stage two.
- **Endless ammo** for either player (rounds at `0x1A3680 + 4p`, capacity
  `0x3C` on) and **add credits** (`0x12D4`, to 99) under Debug.
- `--env FILE` (harness variables from a file) and `--roms DIR`. Netplay
  tested across two machines on a LAN: they stay frame-identical.
- **Playable through stage one.** Attract mode is complete and correct --
  logo, the "something has survived" sequence, the title, rankings and the
  demo -- and a credit plays through to the T-Rex.
- **A menu bar** (from model3recomp): video, controls, cursor, gamepad,
  Sinden border, save states, cheats and multiplayer. The mouse pointer is
  hidden over the game by default.
- **Two players over the network**: `--host PORT` / `--join ADDR:PORT`, or
  the Multiplayer menu. Lockstep, frame-identical on both machines, checked
  by a RAM hash.
- **Save states** at the branch back to the top of the game's state loop
  (`0x2100`); exact across runs.
- **Infinite health** for either player under Debug. Health is a word per
  player at `0x1A3720` and `0x1A377C`, in medkits.
- **High scores and settings persist** in `lostworld.nv`.
- `M3_RAM_EVERY=N,DIR` writes the low 2 MB of RAM every N fields, for
  finding the game's variables by search.
- `docs/hero.gif`, from a played run.
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
- **The game stopped at the zip line** ("Let's take a short cut", stage 4):
  the camera zoomed in and nothing moved on. The cases of a switch at
  0x00044C14 were never recompiled, because model3recomp's lifter only
  scanned for switch tables once. It now repeats the scan until it finds
  nothing new, which also picked up some 70 other missed cases. **Run
  `tools/lift.py` again** to get them.
- See-through texels (smoke, shadows, glows) blend instead of being drawn
  solid or not at all.
- The crosshair is shown by default.
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
- A translucent mist is too strong in places.
