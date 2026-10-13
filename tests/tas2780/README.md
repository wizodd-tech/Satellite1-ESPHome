# TAS2780 Host Tests

Run from the worktree root:

```sh
sh tests/tas2780/run.sh
```

The runner compiles the real `esphome/components/tas2780/tas2780.cpp` with C++17,
`-Wall -Wextra -Wpedantic -Werror`, AddressSanitizer, and UndefinedBehaviorSanitizer.
Unused register constants in the production translation unit are exempted from
warnings. Temporary build output stays under this directory and is removed on exit.
Set `CXX` to choose a compiler or `SANITIZERS=''` to disable sanitizers.

The small mocks provide named timeout replacement/cancellation, a wrapping
32-bit clock, AudioDac's mute state, page-aware registers, ordered I2C events,
and one-shot read/write failures with optional matching-operation skips. Advancing
the clock runs due callbacks at the new time; tests explicitly advance through
each activation/retry deadline. Software reset retains simulated physical supply
samples, restores chip identity, and clears page-0 configuration. Latch clearing
models the driver's existing clear command, not verified silicon semantics.

The fifteen test groups cover cooldown and reset/init bypass prevention, latch-read ordering,
distinct trips, rolling-window boundaries and clock wrap, persistent lockout and
deactivation, cancelled/stale callbacks, entry/completion gates, conservative fault
handling, failed/equal/muted DVC writes, confirmed lower DVC ordering, 0.5 dB scaling,
shared-register mute restoration, and normal bootstrap/fallback on both supply
variants. Runtime-only recovery checks include bootstrap shutdown/read failures
preserving mode-2 fallback, repeated mode-2 failure deactivating without retry or
permanent blocking, pending-loop suppression, public init/reset/repeated activation
detecting runtime faults before the next poll, and reset-invalidated DVC baselines
requiring successful unmuted restoration. Deactivation before polling preserves OC
trip/cooldown without scheduling recovery until explicitly requested; thermal,
unknown, and failed observations remain blocked. Raising DVC again during pending
lockout recovery prevents completion from writing active mode.
No hardware or ESPHome installation is required. These tests do not
validate datasheet bit meanings, real scheduler concurrency, or physical OC safety.
