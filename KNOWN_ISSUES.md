# Known issues

Log 5: 9 dodges / 9 hits, ~40 s against Gunslinger.

## Confirmed not bugs

- `r=127` in `[t*]` is the combined radius: own 120 + projectile raw 7 (`0x74`). The raw value is visible as `r=7` in `[nv]`. The `RADIUS_SCALE` question stays open (see Open), but the arithmetic of the field is consistent.
- `ttc=inf` when `ca` is above the combined radius. Over the whole log `ca < r` maps to a finite `ttc` and `ca > r` to `inf`, without exceptions. No hidden threshold.
- `ca` is computed for the current position, without own movement. That is a modelling limit, not a defect, and it only matters while the bot is moving.

## Confirmed bugs

- Hits land while every listed threat has `ca` in the 160..470 range, i.e. the model predicts a miss but the game registers damage. Either the effective hit radius is larger than 127, or the predicted direction is off for those volleys. `[hit]` now reports the four smallest `ca` at the exact frame of the damage event, which settles it in one run.
- `-2160` in one second means three pellets of a single volley connected, so the bot stood inside the spread cone. Cause is not yet attributed.

## Measured, not assumed

- Scan cadence is set by `SCAN_PERIOD_US = 16667` (60 Hz) in `src/core/scan.cpp`, driven by the render swizzle with a main-queue fallback. The `interval(ms=1000)` string in the `[scan]` line is the counter report period only, and the `[m]`/`[t*]` lines are printed by `diag_tick` once per second. Neither limits the dodge: `autododge()` runs on every scan.
- In-log proof of the cadence: `threatCandidates` reaches 100..300 per second while at most ten projectiles exist. At a 1 Hz scan the ceiling would be ten.
- `[m] hz=` now reports the real number of scan ticks per second, so the cadence is measured instead of argued.

## Open

- Effective hit radius: compare `ca` in `[hit]` against the real damage events. If hits keep arriving at `ca` 250..400, scale `RADIUS_SCALE` by data rather than by guess.
- Projectile velocity is never read from memory. `dir_x`/`dir_y` come from position deltas, magnitude comes from `0x124` with a measured fallback. A projectile with `v=(0,0)` is one seen for the first time, before any delta exists; it is a first-frame artefact, resolved on the next scan.
