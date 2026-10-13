# Protection Recovery

This extracts the recovery intent of `28ecad2a` onto develop `c805ea7c`, without
its supply reconciliation or changes to develop's checked, 0.5 dB DVC scaling
and shared-register mute behavior.

## Policy

- Automatic recovery is runtime-only: the device must have completed activation
  and reached established active playback. A checked runtime software-shutdown
  observation captures all six latch registers before activation/reset clears
  them. Other unexpected runtime modes fail closed.
- Startup and recovery bootstrapping follow develop's existing supply fallback
  and failure path, not OC retry scheduling. A software-shutdown supply sample
  returns false without cancelling activation-pending state, allowing the
  no-external-VBAT mode-0 bootstrap to fall back to mode 2. Bootstrap mode-read
  failures likewise return false to that path. The loop skips mode polling while
  activation is pending or the device is not established active. Startup write
  failures still fail closed; this policy does not complete YAML output readiness
  through an asynchronous startup retry.
- Only an isolated OC bit in LTCH0, with every other latch byte zero and every
  read and shutdown write successful, permits automatic recovery. This deliberately
  rejects even additional informational flags rather than guessing their safety.
- The first two distinct shutdowns in a rolling 60-second window retry after
  at least 2 seconds. A distinct trip requires active, not activation-pending state;
  repeated observations of the same stopped device do not count again.
- The third trip locks out activation until a successful unmuted DVC write is
  strictly more attenuated than the last confirmed unmuted level at that trip.
  Failed writes, floating-point reductions that quantize to the same DVC, and
  mute-only writes cannot unlock it. A volume request while muted can qualify
  later when unmuting actually writes the reduced level. Unknown prior DVC blocks
  recovery until reboot. Unlock resets the trip history, not the 2-second delay.
  The confirmed lower level is rechecked at activation entry and completion;
  raising it again before completion reinstates lockout.
- Activation entry and delayed completion honor recovery gates. Intentional
  deactivation cancels both timers and desired activity, but preserves lockout,
  fault blocking, trip history, and the remaining cooldown. Lowering volume while
  intentionally inactive does not activate the device. Reset cannot bypass gates.
- Public init, reset, and deactivate check hardware MODE_CTRL while software still considers
  the device active, before software reset can clear evidence. Repeated activate
  calls also check an already-active device instead of trusting its cached state.
  An unexpected mode is classified and stops init/reset; a failed read blocks
  recovery. Public init respects recovery gates, while internal startup init
  remains permitted during activation-pending state for supply-mode fallback.
- Hardware init invalidates the last confirmed unmuted DVC baseline. An unmuted
  `write_mute_()` reconfirms it through the checked volume write; a mute-only write
  cannot restore an unmuted baseline. A third runtime trip with an unknown baseline
  intentionally fails closed rather than trusting a level from before reset.
- Thermal, mixed, unknown, or I2C-failed runtime fault observations block all recovery
  until reboot, including volume-lowering recovery. A failed shutdown write can
  leave physical hardware state uncertain; the driver does not claim otherwise.

## Original Review

The original patch is not merge-ready as written: repeated callers could bypass
the timer and lockout; deactivate cleared lockout; volume lowering activated
before confirming a lower hardware DVC; unchecked reads could classify failed
I2C observations as faults; and any shutdown (including thermal/unknown faults)
was retried. It also used a fixed window rather than a rolling trip history.

## Verification Still Required

The independent datasheet review confirms the OT/OC latch bit mappings and
latch-clear bit, and that live flags reset in software shutdown while latches are
retained. Live flags therefore do not establish thermal clearance after shutdown;
thermal automatic recovery remains intentionally unsupported. This is partial
datasheet verification, not proof of overall recovery safety. Before hardware
release, confirm OC retry safety, remaining latch register/read side effects,
power-mode bootstrap behavior, DVC restoration, and shutdown/write-failure
handling on both board supply variants. The strict isolated-OC equality policy
is unchanged; no additional flag combinations are assumed safe.

Recovery bootstrap still follows develop's fallback even if its shutdown was
caused by thermal or another fault rather than missing external VBAT. Establish
the expected missing-VBAT signature and a safe bootstrap fault policy before
approving automatic recovery for release. Runtime fault blocking alone does not
prove that faults arising during the recovery bootstrap are handled safely.

Component-only recovery does not update YAML's cached `tas2780_active` value or
DAC proxy readiness when an asynchronous runtime retry succeeds/fails. It never
claims to repair startup YAML output readiness. Caller/UI state and
bootstrapping's existing unchecked init/configuration writes remain follow-up
concerns. Software review and deterministic tests are prerequisites for merge;
hardware verification is a prerequisite for claiming operational safety.

## Test Interface

Use a TAS2780 test subclass to inspect protected state and call
`finish_activation_()`, `read_supply_voltages_()`, and
`protection_shutdown_(bool)` and `observe_runtime_mode_()`. Fake I2C register
reads/writes, `millis()`, and named timeouts; production test-only APIs are
unnecessary. Exercise public activate,
deactivate, init, reset, volume, and mute calls. Assert latch reads precede clearing,
DVC writes precede activation, rolling-window and clock-wrap behavior, two
bounded retries, third-trip lockout, repeated observations, both activation gates,
deactivate/re-activate, muted/equal/failed volume writes, and all conservative
fault-blocking paths. Extend coverage for bootstrap software shutdown and mode-read
failure preserving mode-2 fallback, pending-loop suppression, no startup OC retry,
runtime reset/public-init/deactivate observation before evidence is cleared, repeated active activation,
reset-invalidated DVC baselines, and unmuted reconfirmation versus mute-only writes.

Run `sh tests/tas2780/run.sh` from the repository root. The harness compiles the
actual component with fake I2C and scheduler implementations; 15 test groups pass
with warnings as errors, AddressSanitizer, and UndefinedBehaviorSanitizer.
`config/satellite1.yaml` validates and compiles with ESPHome 2026.9.1, with generated
TAS2780 sources verified against the local files. These checks do not establish
silicon fault behavior or physical retry safety; no hardware was accessed.
