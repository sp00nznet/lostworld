# Roadmap

The milestone is one specific frame: **The Lost World's attract mode**.

## Next: get the game to submit a scene

Not the Real3D, which is the surprise. The game reaches its main entry, runs
its frame task every field, and draws its own text — and then submits no
geometry whatsoever. Culling RAM holds a viewport and an LOD table and nothing
else; polygon RAM is never written at all. Its own state machine is stalled on
a counter at RAM `0x001A38C0` that never advances.

A rasteriser written before that is fixed has nothing to walk. Find what is
meant to move that counter first.

## Then: the Real3D

Once there is a scene, this is board-level work and belongs in
[model3recomp](https://github.com/sp00nznet/model3recomp), not here.

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
