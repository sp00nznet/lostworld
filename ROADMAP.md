# Roadmap

The milestone is one specific frame: **The Lost World's attract mode**.

## Next: the Real3D

Everything else in the picture already works. The game builds a scene every
frame — the culling and polygon RAM fill up, and the SCSI DMA feeds the
texture port — and nothing draws it.

This is board-level work and belongs in
[model3recomp](https://github.com/sp00nznet/model3recomp), not here:

- Walk the culling-RAM node tree, accumulating transforms.
- Parse the display list out of polygon RAM.
- Decode models and textures from the VROM.
- Transform, light, clip and rasterise.

## Then, in this repository

- Wire the inputs so coins and the trigger reach the game.
- A conformance run: fixed field counts, captured framebuffer checksums,
  tracked over time so a regression is visible.
- Sound, once the board has a 68000 and SCSPs.

## Deferred

- The other Lost World revision (`lostwsgo`).
- Anything requiring the security board — this title does not use it.

## Out of scope

- Being an emulator. Use Supermodel; this project is built from the same
  public research.
- Distributing anything derived from the ROM.
