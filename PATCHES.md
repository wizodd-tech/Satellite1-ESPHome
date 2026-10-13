# Wizodd patches to the Satellite1 firmware

This branch (`wizodd-patched`) is FutureProofHomes' Satellite1 firmware **v0.2.1** plus fixes for one
problem: a Satellite1 that stops answering its wake word until it is restarted.

Upstream issue: [FutureProofHomes/Satellite1-ESPHome#540](https://github.com/FutureProofHomes/Satellite1-ESPHome/issues/540).
The faults are in ESPHome's `voice_assistant` component (checked against 2026.7.3 and `dev`), so that
component is carried here in `esphome/components/voice_assistant/` and overrides the built-in one.
The first commit on the branch adds it unmodified; the second is the fix, so `git show` on it is the whole patch.

## What goes wrong

### 1. The unit plays its wake sound, then nothing (red twinkling ring)

1. The unit allows one voice subscriber and remembers it by its API connection.
2. Wi-Fi drops without a clean TCP close. The unit keeps the dead connection until its keepalive gives up,
   2.5 x 60 s = 150 s later.
3. Home Assistant reconnects within seconds and subscribes again. `client_subscription()` sees a subscriber
   already present and refuses, with only a log line (`Multiple API Clients attempting to connect to Voice
   Assistant`). Home Assistant is not told.
4. The dead connection is reaped. Now nobody is subscribed, `on_client_disconnected` puts the unit in its
   "not ready" phase, and Home Assistant never subscribes again because it believes it already is.
5. A wake word still plays the wake sound, but `request_start()` returns at `No API client connected`.

Seen from Home Assistant: the device is online, but its *Wake word* select is `unavailable` and no pipeline
run is recorded when you speak.

### 2. The unit is silently deaf

Three states wait for something that may never come, with no timeout: `STARTING_PIPELINE` (Home Assistant's
answer to the start request), `AWAITING_RESPONSE` (the run's events) and `STREAMING_RESPONSE` when the reply
URL was handed to the media player but the announcement never began. While the state is not `IDLE`,
`voice_assistant.is_running` is true, and the Satellite1 wake word handler then calls `voice_assistant.stop`
instead of starting a run. In those states `stop` only asks the other side to wind down, so the state never
changes and every later wake word does the same.

## What this branch changes

| Change | File |
|---|---|
| A new subscriber replaces the current one instead of being refused. The interrupted run is aborted and `on_client_disconnected` then `on_client_connected` fire, as for a real reconnect. | `voice_assistant.cpp`, `client_subscription()` |
| Stall timeouts: 15 s for the start request, 5 min for the reply and for reply playback to begin. On expiry the run is ended and `on_error` (`pipeline-timeout`) and `on_end` fire. The 5 min figure is deliberately long so a slow conversation agent is not cut off. | `voice_assistant.cpp`, `run_is_stalled_()`, `loop()` |
| After `voice_assistant.stop`, the run must reach idle within 5 s or it is ended by force. A stuck unit therefore recovers on the first wake word and answers the second. | `voice_assistant.cpp`, `request_stop()` |
| Audio ducking is released when the client disconnects, since a run cut off that way never reaches `on_end`. | `config/common/voice_assistant.yaml` |
| The build uses the patched component; firmware updates are read from this repository's `manifest.json` so Home Assistant does not offer the unpatched build as an "update". | `config/satellite1.yaml`, `config/common/components.external.yaml` |

## Status

- Builds with ESPHome 2026.7.3 (`esphome compile config/satellite1.yaml`).
- Only the base image (`satellite1.yaml`) is built and published. The radar variants (`ld2410`, `ld2450`)
  compile from the same sources but still point at FutureProofHomes' update manifests.
- See the release notes for what has been tested on hardware.

## Installing

Download `satellite1.ota.bin` from the latest release, then from a machine on the same network:

    pip install esphome==2026.7.3
    git clone -b wizodd-patched https://github.com/wizodd-tech/Satellite1-ESPHome.git
    cd Satellite1-ESPHome
    esphome upload config/satellite1.yaml --device <satellite ip> --file satellite1.ota.bin

Wi-Fi settings and the Home Assistant pairing are kept. To go back, install FutureProofHomes' firmware from
<https://docs.futureproofhomes.net>.

## Keeping up with upstream

When FutureProofHomes moves to a newer ESPHome, replace `esphome/components/voice_assistant/` with that
version's files and re-apply the fix commit. If ESPHome fixes `client_subscription()` and adds the timeouts
itself, delete the directory and the `voice_assistant` entries in the two `external_components` lists.
