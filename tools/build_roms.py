"""Build The Lost World's ROM images from a romset.

  python tools/build_roms.py <lostwsga.zip|romdir> [outdir]

A thin wrapper over the board-level loader in ext/model3recomp, so the game
repo documents its own inputs without duplicating the interleave logic. The
loader proves the program-ROM layout against the PowerPC exception vector
table rather than assuming a chip order.

Writes lw_prog.bin, lw_bank.bin and lw_vrom.bin, and the sound board's
lw_snd.bin (the 68000 program) and lw_samples.bin (the SCSP wave ROM). None
of them belong in git.
"""
import os
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
LOADER = os.path.join(HERE, "..", "ext", "model3recomp", "tools",
                      "rom_loader.py")


# The sound board's chips, by name: each is a 16-bit ROM stored low byte
# first, so a byte swap gives the 68000's big-endian view (MAME's
# ROM_LOAD16_WORD_SWAP).
SOUND = [("lw_snd.bin", ["epr-19940.21"]),
         ("lw_samples.bin", ["mpr-19934.22", "mpr-19935.24"])]


def read_chip(src, name):
    if os.path.isdir(src):
        with open(os.path.join(src, name), "rb") as f:
            return f.read()
    with zipfile.ZipFile(src) as z:
        return z.read(name)


def build_sound(src, out):
    for image, chips in SOUND:
        data = bytearray(b"".join(read_chip(src, c) for c in chips))
        data[0::2], data[1::2] = data[1::2], data[0::2]
        with open(os.path.join(out, image), "wb") as f:
            f.write(data)
        print("%-15s: %s  (0x%X bytes from %d chip%s)" % (
            "sound" if image == "lw_snd.bin" else "samples",
            os.path.join(out, image), len(data), len(chips),
            "" if len(chips) == 1 else "s"))


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

    rc = subprocess.call([
        sys.executable, LOADER, src,
        os.path.join(out, "lw_prog.bin"),
        "--banked", os.path.join(out, "lw_bank.bin"),
        "--vrom", os.path.join(out, "lw_vrom.bin"),
    ])
    if rc == 0:
        build_sound(src, out)
    return rc


if __name__ == "__main__":
    sys.exit(main())
