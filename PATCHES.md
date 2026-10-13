# Wizodd patches to the Satellite1 firmware

> **Unofficial.** This is not FutureProofHomes firmware. It is their open-source Satellite1 firmware **v0.2.1**
> with a small set of fixes applied by Wizodd Technologies. FutureProofHomes has not reviewed, approved or
> endorsed it. Read [Before you install](#before-you-install) and
> [Going back to the original firmware](#going-back-to-the-original-firmware) first.

## What this is

Branch `wizodd-patched` fixes one problem: a Satellite1 that stops answering its wake word until it is
restarted ([FutureProofHomes/Satellite1-ESPHome#540](https://github.com/FutureProofHomes/Satellite1-ESPHome/issues/540)).
Nothing else about how the device behaves is meant to change.

The faults are in ESPHome's `voice_assistant` component (checked against 2026.7.3 and `dev`), so that
component is carried here in `esphome/components/voice_assistant/` and overrides the built-in one.
The first commit on the branch adds it unmodified; the second is the fix, so `git show` on it is the whole patch.

## Before you install

Things that can go wrong, or that you give up, by running this firmware:

**Support**

- FutureProofHomes did not write or test these changes. They may decline to help with a unit that runs this
  firmware, or ask you to put theirs back first. Whether third-party firmware affects your warranty is
  between you and FutureProofHomes; ask them if it matters to you.
- Wizodd Technologies provides no support, and may stop maintaining this branch at any time.

**Updates**

- A unit on this firmware reads its updates from this repository, not from FutureProofHomes. Home Assistant
  will **not** offer you FutureProofHomes' future releases, including their bug and security fixes, until
  you go back to their firmware.
- The *Beta firmware* switch on the device no longer selects FutureProofHomes' beta builds. Both positions
  read this repository.
- If this repository is removed or renamed, the update check on the unit fails. The unit keeps working.

**Testing**

- Tested on five Satellite1 units in one home since 2026-10-13, all the base model on firmware v0.2.1, with
  Home Assistant 2026.10. That is the whole test history.
- The recovery code only runs when a connection fails, so it is the least exercised part.
- There is no automated test suite.

**Behaviour that differs from the original**

- The newest voice subscriber wins. If more than one client subscribes to the satellite's voice assistant
  (a second Home Assistant, a custom client), each new subscription takes it from the previous one. The
  original firmware refuses the newcomer instead.
- A voice run with no progress for 5 minutes is ended, and an error is shown on the LED ring. A conversation
  agent that takes longer than 5 minutes to start answering is cut off.
- After a stop request, a run that has not ended within 5 seconds is ended by force.
- This does **not** stop Wi-Fi dropouts. It changes what happens after one.

**Hardware**

- Only the base image is published. **Do not install it on a unit that runs the radar variants**
  (`ld2410`, `ld2450`; the installed version ends in `+ld2410` or `+ld2450`): you would lose the radar
  functions.
- Built for the Satellite1 Core Board (ESP32-S3) only.

**Flashing**

- An over-the-air update that fails normally leaves the running firmware in place. As with any firmware
  update, there is a small chance a unit ends up needing a USB re-flash.
- Wi-Fi settings and the Home Assistant pairing are kept across the update in our use. Have your Wi-Fi
  password and the device's Home Assistant entry to hand anyway.

**Binaries**

- The published `.bin` files were built by Wizodd Technologies on our own machine. They are not signed and
  the build is not reproducible bit for bit. If you would rather not trust them, build from source
  (`esphome compile config/satellite1.yaml` with ESPHome 2026.7.3).

## Installing

Download `satellite1.ota.bin` from the [latest release](https://github.com/wizodd-tech/Satellite1-ESPHome/releases),
then from a machine on the same network as the satellite:

    pip install esphome==2026.7.3
    git clone -b wizodd-patched https://github.com/wizodd-tech/Satellite1-ESPHome.git
    cd Satellite1-ESPHome
    esphome upload config/satellite1.yaml --device <satellite ip> --file satellite1.ota.bin

Afterwards the device's firmware entity in Home Assistant shows the installed version with a `-wizodd.N` suffix.

## Going back to the original firmware

Either method returns the unit to FutureProofHomes' firmware. From then on it reads FutureProofHomes'
updates again and Home Assistant offers their releases as before.

**Over Wi-Fi** (keeps Wi-Fi settings and the Home Assistant pairing):

    pip install esphome==2026.7.3
    git clone -b v0.2.1 https://github.com/FutureProofHomes/Satellite1-ESPHome.git
    cd Satellite1-ESPHome
    curl -LO https://raw.githubusercontent.com/FutureProofHomes/Documentation/refs/heads/main/assets/firmware/esphome/production/satellite1.ota.bin
    esphome upload config/satellite1.yaml --device <satellite ip> --file satellite1.ota.bin

That URL is FutureProofHomes' current stable build, the same file their own update mechanism installs. Its
MD5 is listed in their [manifest](https://raw.githubusercontent.com/FutureProofHomes/Documentation/refs/heads/main/manifest.json)
if you want to check the download.

**Over USB** (works even if the unit will not start or join Wi-Fi): follow FutureProofHomes'
[Flashing Firmware](https://docs.futureproofhomes.net/satellite1-flash-via-usb-c/) guide. If the installer
offers to erase the device and you accept, you will have to set up Wi-Fi and add the device to Home
Assistant again.

There is no way to go back from the Home Assistant update screen, because on this firmware that screen
reads this repository.

## What goes wrong in the original

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
| The build uses the patched component; firmware updates are read from this repository's `manifest.json`. | `config/satellite1.yaml`, `config/common/components.external.yaml` |

## Status

- Builds with ESPHome 2026.7.3 (`esphome compile config/satellite1.yaml`).
- Only the base image (`satellite1.yaml`) is built and published. The radar variants compile from the same
  sources but are not published, and their update check still points at FutureProofHomes.
- The release notes say what has been tested on hardware.

## Keeping up with upstream

When FutureProofHomes moves to a newer ESPHome, replace `esphome/components/voice_assistant/` with that
version's files and re-apply the fix commit. If ESPHome fixes `client_subscription()` and adds the timeouts
itself, delete the directory and the `voice_assistant` entries in the two `external_components` lists. At
that point this branch has no reason to exist and units should go back to FutureProofHomes' firmware.

## Disclaimer

This firmware and the binaries built from it are provided "as is", without warranty of any kind, express or
implied, including but not limited to the warranties of merchantability, fitness for a particular purpose
and non-infringement. You install and use them at your own risk. In no event shall Wizodd Technologies or
the contributors be liable for any claim, damages or other liability, including damage to or loss of use of
a device, arising from the firmware or its use. The licence in `LICENSE` governs; this paragraph restates
it and does not replace it.

Wizodd Technologies is not affiliated with, sponsored by or endorsed by FutureProofHomes, the ESPHome
project, the Open Home Foundation, Nabu Casa, Espressif or XMOS. Satellite1, FutureProofHomes, ESPHome,
Home Assistant and other names used here are trademarks of their respective owners and are used only to
say what this software works with.

## Licence and credits

Same licence as the original: the ESPHome License (MIT for the C++/runtime code, GPLv3 for the Python code),
in [`LICENSE`](LICENSE). The firmware is the work of FutureProofHomes and the ESPHome contributors; the
changes on this branch are a few dozen lines on top of theirs. Copyright notices, the list of modified files
and the third-party software contained in the binaries are in [`NOTICE.md`](NOTICE.md).
