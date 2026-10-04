# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- **English.** A Region option (Game menu, USA by default) sets the country
  byte the game picks its text by -- RAM `0x1226`, the EEPROM settings'
  copy. The Japanese board carries the American attract screens, subtitles
  and prompts; no other dump is needed.
- **The HUD**: the ammunition counter, the RELOAD prompt, pickups and the
  T-Rex's target circles, which were being painted over (model3recomp's
  viewport priority fix). A scripted run with infinite health beats the
  T-Rex and reaches stage two.
- **Endless ammo** for either player (rounds at `0x1A3680 + 4p`, capacity
  `0x3C` on) and **add credits** (`0x12D4`, to 99) under Debug.
- **A two-machine netplay test on recomp-netlab**: `--env FILE` and
  `--roms DIR` for its run line, `tools/netlab/*.env` for the two players'
  scripts. It passes: the host here and a lab VM stay identical.
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
- No sound.
