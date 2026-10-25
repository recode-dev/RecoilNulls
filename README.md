# Recoil

Tweak for **Nulls Brawl** (iOS, arm64): trigger assist and projectile dodge.

This is the Nulls-Recoil codebase: the same sources, the same hook layer, the
same offsets. Nothing is reimplemented.

## Layout

```
tools/       hook engine (pointer-slot install, ObjC swizzle, mach helpers)
src/
  recoil.h   umbrella header
  core/      offsets, scan, commands
  helpers/   dodge kinds, profiles, aim ahead, assist
  features/  autoaim, autododge
  utils/     walls, brawlers, crypto, flags
```

## Build

```
make -j"$(( $(nproc) + 1 ))" ARCHS=arm64 DEBUG=0 FINALPACKAGE=1
```

CI builds on every push to `test` and uploads `Recoil.dylib`.

## Install

1. LiveContainer → **Tweaks** → **Add** → `Recoil.dylib`
2. Sign it there
3. Launch the game

## License

MIT — see `LICENSE`.
