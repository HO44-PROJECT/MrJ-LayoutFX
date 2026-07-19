# MrJ-LayoutFX — documentation

Model-railway lighting & signalling for Arduino/ESP32: declarative JSON layout,
a self-served web control panel, and a rich set of signal/lamp/servo effects.

> Documentation is in **English** for now. A French translation may follow.

Docs are organized by **who you are**, not by topic — pick your section below.

## [user/](user/) — flash it, run it, drive it from the browser

For anyone connecting an ESP32 (or an off-the-shelf MrJ layout board) and
controlling it day-to-day. No PlatformIO, no C++.

| Page | What's in it |
|---|---|
| [about.md](user/about.md) | What this project does, what it can drive, the big picture. |
| [getting-started.md](user/getting-started.md) | Flash a device (browser installer or from source) and reach the WebUI for the first time. |
| [usage.md](user/usage.md) | Every screen and button in the WebUI: cockpit, boards & extensions, buses, diagnostics, About/OTA. |
| [device-types.md](user/device-types.md) | Every device type the WebUI's device editor can create, grouped by category. |
| [config-examples.md](user/config-examples.md) | Three ready-to-use `config.json` files, from simplest to most elaborate. |

## [advanced/](advanced/) — reconfigure the firmware yourself

For building your own firmware image: your own WiFi credentials, a different
board, compile-time feature flags, or driving the device from DCC/the REST API
instead of the WebUI.

| Page | What's in it |
|---|---|
| [building-from-source.md](advanced/building-from-source.md) | PlatformIO build, `config.h`, flashing firmware + filesystem. |
| [configuration-flags.md](advanced/configuration-flags.md) | **Every `config.h` flag**, its type, default and effect. |

## [contributing/](contributing/) — changing the code itself

For anyone opening a PR against the firmware: project conventions, how the
build pipeline works, and how the runtime is put together internally.

- [conventions.md](contributing/conventions.md), [platformio.md](contributing/platformio.md),
  [build-pipeline.md](contributing/build-pipeline.md),
  [adding-a-device-type.md](contributing/adding-a-device-type.md) — the project's
  rules and mechanics: source layout, the PlatformIO model, the pre-build
  code-generation pipeline, and how to add a new device type end to end.
- [architecture/](contributing/architecture/) — how the firmware **runs**:
  config-driven model, concurrency, memory frugality.
- [troubleshooting/](contributing/troubleshooting/) — investigation logs of real
  debugging sessions: what was tested, ruled out and concluded, kept so the same
  ground is never covered twice.

## In-source references

- [`include/LayoutFX_define.h`](../include/LayoutFX_define.h) — user flags → `LFX_*_ENABLED`.
- [`include/LayoutFX_default.h`](../include/LayoutFX_default.h) — default values and effect tuning constants.

## Conventions used in this doc

- **Toggle flag** — `#define X` to enable; omit to disable (default off).
- **Value flag** — `#define X <value>`; the documented default applies when omitted.
- **Built vs active** — a WebUI feature badge is green when a flag is *compiled in*;
  whether the feature is *active* is shown in context (e.g. the Bus tab).
