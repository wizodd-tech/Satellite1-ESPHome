# Firmware binaries

Built from the [`wizodd-patched`](https://github.com/wizodd-tech/Satellite1-ESPHome/tree/wizodd-patched) branch
with ESPHome 2026.7.3. `manifest.json` on that branch points here, which is how a unit already running this
firmware finds its updates.

**Unofficial.** This is FutureProofHomes' open-source Satellite1 firmware with fixes applied by Wizodd
Technologies LLC. FutureProofHomes has not reviewed, approved or endorsed it, and Wizodd Technologies LLC is not
affiliated with them.

Before installing, read these two sections of
[PATCHES.md](https://github.com/wizodd-tech/Satellite1-ESPHome/blob/wizodd-patched/PATCHES.md):

- [Before you install](https://github.com/wizodd-tech/Satellite1-ESPHome/blob/wizodd-patched/PATCHES.md#before-you-install): what can go wrong and what you give up.
- [Going back to the original firmware](https://github.com/wizodd-tech/Satellite1-ESPHome/blob/wizodd-patched/PATCHES.md#going-back-to-the-original-firmware).

| File | Use |
|---|---|
| `<version>/satellite1.ota.bin` | Over-the-air update of a base-model Satellite1 already running Satellite1 firmware |
| `<version>/satellite1.factory.bin` | Full image for flashing over USB |

Base model only. Do not install on a unit running the radar variants (`+ld2410`, `+ld2450`).

## Licence

The binaries are distributed under the ESPHome License in [`LICENSE`](LICENSE). They contain third-party
software, listed with its licences in [`NOTICE.md`](NOTICE.md), including an XMOS firmware image under the
XMOS Public Licence in [`licenses/XMOS-PUBLIC-LICENCE.txt`](licenses/XMOS-PUBLIC-LICENCE.txt) and sound effects
by Clayton Charles Tapp under CC BY 4.0 ([`licenses/SOUNDS-CC-BY-4.0.md`](licenses/SOUNDS-CC-BY-4.0.md)). The source for
everything built here is the `wizodd-patched` branch of this repository.

Provided "as is", without warranty of any kind. You install and use these files at your own risk.
