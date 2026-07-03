# MrJ-RailwayFX — documentation

Model-railway lighting & signalling for Arduino/ESP32: declarative JSON layout,
a self-served web control panel, and a rich set of signal/lamp/servo effects.

> Documentation is in **English** for now. A French translation may follow.

## Contents

| Page | What's in it |
|---|---|
| [about.md](about.md) | Project goal, vision, what it can drive, high-level architecture. |
| [setup.md](setup.md) | Build your first firmware, `config.h`, flash, first boot, recovery. |
| [usage.md](usage.md) | Day-to-day operation from the WebUI: cockpit, config, About, OTA. |
| [configuration-flags.md](configuration-flags.md) | **Every `config.h` flag**, its type, default and effect. |

## Developer documentation

Deeper, dev-facing notes live in three parallel sections:

- [architecture/](architecture/) — how the firmware **runs**: config-driven model,
  concurrency, memory frugality.
- [workshop/](workshop/) — how the firmware is **built**: the PlatformIO model, the
  pre-build code-generation pipeline, and project conventions.
- [troubleshooting/](troubleshooting/) — investigation logs of real debugging
  sessions: what was tested, ruled out and concluded, kept so the same ground is
  never covered twice.

## In-source references

- [`include/LayoutFX_define.h`](../include/LayoutFX_define.h) — user flags → `LFX_*_ENABLED`.
- [`include/LayoutFX_default.h`](../include/LayoutFX_default.h) — default values and effect tuning constants.

## Conventions used in this doc

- **Toggle flag** — `#define X` to enable; omit to disable (default off).
- **Value flag** — `#define X <value>`; the documented default applies when omitted.
- **Built vs active** — a WebUI feature badge is green when a flag is *compiled in*;
  whether the feature is *active* is shown in context (e.g. the Bus tab).
