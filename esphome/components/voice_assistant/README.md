# voice_assistant (patched copy)

This directory is ESPHome's `voice_assistant` component, copied from
[ESPHome 2026.7.3](https://github.com/esphome/esphome/tree/2026.7.3/esphome/components/voice_assistant).
It is listed in `external_components`, so it replaces the component built into ESPHome when this firmware is
compiled.

| File | State |
|---|---|
| `__init__.py` | Unmodified. GPLv3 part of the ESPHome License. |
| `voice_assistant.cpp`, `voice_assistant.h` | **Modified by Wizodd Technologies on 2026-10-13.** MIT part of the ESPHome License. |

What was changed and why is in [`PATCHES.md`](../../../PATCHES.md) at the top of the repository.
`git log -p -- esphome/components/voice_assistant` shows the changes line by line: the first commit is the
unmodified copy, the next is the modification.

It must be built with the ESPHome version it was copied from. See "Keeping up with upstream" in `PATCHES.md`.
