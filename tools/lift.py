"""Recompile The Lost World from the ROM images into src/recomp/.

  python tools/lift.py [--fields N] [--entries FILE]

Model 3 games do not run from ROM. The boot copies the game out of CROM into
RAM and jumps into it, and the copy is not a straight image, so what code
exists and where cannot be recovered from the ROM alone. This runs the machine
before it lifts it:

  1. ext/model3recomp/tools/ppc_interp.py boots from the reset vector against
     the same memory map the runtime provides, and stops when the guest
     branches into RAM.
  2. The RAM snapshot is what gets lifted, at RAM addresses.
  3. The ROM half -- the boot code itself -- is lifted separately, because the
     recompiled program has to start where the processor does.

Both halves land in src/recomp/ and are registered by src/main.c. Neither is
committed: they are derived from your dump, not from this repository.

Targets the lifter could not resolve statically -- a jump-table arm, a handler
address the game builds at runtime -- show up as named func-table misses when
you run the game. Feed them back with --entries and lift again; entering at a
mid-function address is legal, because the CPU context is a global rather than
a parameter.
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
EXT = os.path.join(ROOT, "ext", "model3recomp", "tools")
ROMS = os.path.join(ROOT, "roms")
WORK = os.path.join(ROOT, "work")
OUT = os.path.join(ROOT, "src", "recomp")

PROG = os.path.join(ROMS, "lw_prog.bin")
SNAPSHOT = os.path.join(WORK, "ram.bin")

# Where the program ROM sits in the address map. A 2 MB image occupies the top
# of the 8 MB fixed-CROM window, which is why the reset vector lands at
# 0xFFF00100.
ROM_BASE = "0xFFE00000"


def run(args):
    print("  " + " ".join(os.path.basename(a) if a.endswith(".py") else a
                          for a in args[1:]))
    return subprocess.call(args)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--entries",
                    help="file of extra entry addresses, one per line")
    ap.add_argument("--max", default="60000000",
                    help="interpreter instruction budget for the boot")
    a = ap.parse_args()

    if not os.path.exists(PROG):
        print("No %s. Build the ROM images first:\n"
              "    python tools/build_roms.py /path/to/lostwsga.zip roms/"
              % PROG, file=sys.stderr)
        return 2
    if not os.path.isdir(EXT):
        print("ext/model3recomp is empty -- run:\n"
              "    git submodule update --init --recursive", file=sys.stderr)
        return 2

    os.makedirs(WORK, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)

    print("boot: running to the hand-off into RAM")
    rc = run([sys.executable, os.path.join(EXT, "ppc_interp.py"), PROG,
              "--base", ROM_BASE, "--max", a.max, "--snapshot", SNAPSHOT])
    if rc or not os.path.exists(SNAPSHOT):
        print("boot interpreter did not produce a snapshot", file=sys.stderr)
        return 1

    extra = ["--entries", a.entries] if a.entries else []

    print("lift: boot code (from ROM)")
    rc = run([sys.executable, os.path.join(EXT, "ppc_lifter.py"), PROG, OUT,
              "lwrom", "--base", ROM_BASE])
    if rc:
        return rc

    print("lift: game (from the RAM snapshot)")
    rc = run([sys.executable, os.path.join(EXT, "ppc_lifter.py"), SNAPSHOT,
              OUT, "lwram", "--base", "0x00000000"] + extra)
    if rc:
        return rc

    print("\nsrc/recomp/ is ready. Build with:")
    print("    cmake -S . -B build && cmake --build build --config Release")
    return 0


if __name__ == "__main__":
    sys.exit(main())
