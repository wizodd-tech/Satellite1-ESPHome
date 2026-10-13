# Notices

This repository is a fork of [FutureProofHomes/Satellite1-ESPHome](https://github.com/FutureProofHomes/Satellite1-ESPHome)
and is distributed under the same licence, the ESPHome License in [`LICENSE`](LICENSE): MIT for the
C++/runtime code, GPLv3 for the Python code.

It is an unofficial fork. Wizodd Technologies LLC is not affiliated with, sponsored by or endorsed by
FutureProofHomes, the ESPHome project, the Open Home Foundation, Nabu Casa, Espressif or XMOS. Product and
project names are trademarks of their respective owners.

## Copyright

- ESPHome: Copyright (c) 2019 ESPHome.
- Satellite1 firmware configuration and components: FutureProofHomes and the contributors to that repository.
- Changes on the `wizodd-patched` branch: Copyright (c) 2026 Wizodd Technologies LLC, released under the same
  licence as the files they change.

## Files changed by Wizodd Technologies LLC

All changes were made on 2026-10-13. `git log v0.2.1..wizodd-patched`
shows them exactly.

| File | Change |
|---|---|
| `esphome/components/voice_assistant/__init__.py` | Added. Copied unmodified from ESPHome 2026.7.3. |
| `esphome/components/voice_assistant/voice_assistant.cpp`, `voice_assistant.h` | Added from ESPHome 2026.7.3, then **modified** (see `PATCHES.md`). |
| `config/common/voice_assistant.yaml` | Modified: release audio ducking on client disconnect. |
| `config/satellite1.yaml`, `config/common/components.external.yaml`, `config/satellite1.dashboard.yaml`, `config/satellite1.base.yaml` | Modified: build the patched component; read updates and packages from this repository. |
| `README.md` | Modified: notice added at the top. |
| `PATCHES.md`, `NOTICE.md`, `manifest.json`, `licenses/`, `esphome/components/voice_assistant/README.md` | Added. |

## What the firmware binaries contain

The `.bin` files in the releases and on the `firmware` branch are built from the `wizodd-patched` branch with
ESPHome 2026.7.3. Besides the code in this repository they contain third-party software and data that the
build fetches, the same as FutureProofHomes' own v0.2.1 build. The main parts:

| Part | Licence | Source |
|---|---|---|
| ESPHome runtime and components | ESPHome License (MIT part) | <https://github.com/esphome/esphome> |
| ESP-IDF and the components it bundles (FreeRTOS, lwIP, Mbed TLS and others) | Apache-2.0 for ESP-IDF; the bundled components keep their own permissive licences | <https://github.com/espressif/esp-idf> |
| TensorFlow Lite Micro | Apache-2.0 | <https://github.com/espressif/esp-tflite-micro> |
| microWakeWord models "Hey Jarvis", "Okay Nabu" and "Stop", by Kevin Ahrendt | Apache-2.0, as published in <https://github.com/esphome/micro-wake-word-models> | fetched from FutureProofHomes' firmware assets |
| XMOS firmware image v1.0.3 for the Satellite1 HAT, embedded unmodified | XMOS Public Licence, Version 1: [`licenses/XMOS-PUBLIC-LICENCE.txt`](licenses/XMOS-PUBLIC-LICENCE.txt) | source code: <https://github.com/FutureProofHomes/Satellite1-XMOS> |
| Sound effects (wake, mute, timer, jack and reset sounds), embedded unmodified | As provided by FutureProofHomes for this firmware. We found no separate licence statement for these files. | fetched from FutureProofHomes' firmware assets |

The source code of the XMOS firmware is available under the terms of the XMOS Public Licence at the address
above.

This list covers the main parts and is not exhaustive. If you believe something here is wrong or missing,
contact the repository owner, wizodd-tech, on GitHub.

## No warranty

The software and the binaries are provided "as is", without warranty of any kind. See `LICENSE` and the
Disclaimer in `PATCHES.md`.
