# Recoil

Tweak for **Nulls Brawl** (iOS, arm64) — client-side aim and dodge assist.

No Frida, no instruction patching. Everything is either a guarded memory read or
a pointer-slot swap inside writable segments, so a wrong offset degrades into a
`false` return instead of a crash.

## Layout

```
tools/          hooking primitives, no game knowledge
  image.*       dyld image and segment lookup
  memory.*      guarded read/write, write windows, plausibility checks
  hook.*        pointer-slot scanning, install/uninstall, prologue probes
  objc.*        class and method lookup
  swizzle.*     table driven swizzling with original IMP chain

src/
  module.hpp    install / frame pump / feature flags
  index.cpp     entry point and frame drivers
  recoil.hpp    umbrella header
  core/
    offsets.hpp   all RVAs and struct offsets for one game build
    functions.*   RVA to function-pointer binding
    scan.*        world scan: own character, teammates, enemies, projectiles
    commands.*    client input commands
  features/
    autoaim.*     target selection and shot leading
    autododge.*   projectile threat evaluation and movement
  utils/
    walls.*       tile grid cache, line of sight
    crypto.*      sha256 for the input hash
```

## How hooking works

`tools::hook::install(base, rva, replacement)` finds every data pointer in the
image that references `base + rva` (vtables, function tables) and rewrites it to
`replacement`. The call-through pointer is returned so the replacement can run
the original first or last. `tools::hook::uninstall()` restores every slot.

Replacements take eight `uint64_t` arguments: on arm64 the first eight argument
registers are passed through untouched, so the ABI stays correct whatever the
real signature is.

## Build

```
Actions → Recoil → Run workflow
```

or locally with theos:

```
make -j"$(( $(nproc) + 1 ))" ARCHS=arm64 DEBUG=0 FINALPACKAGE=1
```

Offsets in `src/core/offsets.hpp` are RVAs for one exact build. Other versions
will not work — `tools::hook::site_hash` is there to reject drifted offsets
before they are used.

## Install

1. LiveContainer → **Tweaks** → **Add** → `Recoil.dylib`
2. Sign it with the button at the top of that screen — unsigned, it won't load
3. Launch the game

## License

MIT — see `LICENSE`.
