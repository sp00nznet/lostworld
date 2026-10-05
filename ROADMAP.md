# Roadmap

The first milestone was one frame of attract mode. The game now plays
through stage one to the T-Rex, alone or with a second player over the
network, and saves and loads its state. What is left, in the order a player
would notice it:

## Next: what is still wrong on screen

- **Washed-out mist.** A translucent sheet is drawn too strong in places --
  probably fog or a blend mode the renderer does not do yet.

## Then: netplay beyond the LAN

Two machines on a LAN stay identical. Next is a pair across Tailscale and
one through a forwarded port.

## More cheats

One-hit kills need enemy health found the same way the others were -- a save
state at the right moment and a search over `M3_RAM_EVERY` dumps.

## Deferred

- The other Lost World revision (`lostwsgo`).
- Anything requiring the security board — this title does not use it.

## Out of scope

- Being an emulator.
- Distributing anything derived from the ROM.
