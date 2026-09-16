"""Build The Lost World's ROM images from a romset.

  python tools/build_roms.py <lostwsga.zip|romdir> [outdir]

A thin wrapper over the board-level loader in ext/model3recomp, so the game
repo documents its own inputs without duplicating the interleave logic. The
loader proves the program-ROM layout against the PowerPC exception vector
table rather than assuming a chip order.

Writes lw_prog.bin, lw_bank.bin and lw_vrom.bin. None of them belong in git.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LOADER = os.path.join(HERE, "..", "ext", "model3recomp", "tools",
                      "rom_loader.py")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "roms")
    os.makedirs(out, exist_ok=True)

    if not os.path.exists(LOADER):
        print("ext/model3recomp is empty -- run:\n"
              "    git submodule update --init --recursive", file=sys.stderr)
        return 2

    return subprocess.call([
        sys.executable, LOADER, src,
        os.path.join(out, "lw_prog.bin"),
        "--banked", os.path.join(out, "lw_bank.bin"),
        "--vrom", os.path.join(out, "lw_vrom.bin"),
    ])


if __name__ == "__main__":
    sys.exit(main())
