# The PlatformIO project model

[Docs](../README.md) / [Contributing](README.md) / PlatformIO project model

The firmware targets several boards (ESP32 variants and the AVR Nano) from a
single tree. `platformio.ini` (in the main project, not the library) expresses
that with a small set of reusable **template sections** and a longer list of
buildable **environments** that pull the templates in. This document explains the
mechanics so the `.ini` reads as a system rather than a pile of special cases.

## The build directory lives off Google Drive

The project tree is synced by Google Drive File Stream. A build writes thousands
of tiny temporary files (`.o`, `.d`, …), and the Drive virtual filesystem is slow
and flaky under that load — it surfaces as random
`opening dependency file …​.d: No such file or directory` errors. We also don't
want gigabytes of object files syncing to the cloud. So `[platformio]` redirects
all build artifacts out of the synced tree:

```ini
[platformio]
build_dir = /Users/frip/pio-builds/MrJ-ArduinoRailwayFX
```

The path is **machine-specific**. On another machine, override it with the
`PLATFORMIO_BUILD_DIR` environment variable instead of editing the file.

## Template sections vs. environments

Only `[env:...]` sections are buildable (they show up in `pio run`). Every other
`[name]` section is a **template**: a named bag of options referenced elsewhere,
either by `${section.option}` interpolation or by `extends`. Templates never build
or flash on their own.

| Template | Purpose |
| --- | --- |
| `[dbg]` | Deep debug flags — add `${dbg.build_flags}` to an env to enable heap poisoning, stack checks, verbose core log. |
| `[esp32]` | Common ESP32 flags (`-DESP32`, `-Wall`, `-Wextra`, core debug level). |
| `[esp32_optimized]` | The trimmed partition table + LittleFS filesystem (catalogs live in PROGMEM, so LittleFS can be minimal). |
| `[libs_esp32]` / `[libs_avr]` | The standard library sets per architecture, reused via `${libs_esp32.lib_deps}`. |
| `[esp32_webui_base]` | The full ESP32 + WebUI + JSON-config template that most ESP32 environments `extends`. |

`[esp32_webui_base]` is deliberately not an `[env:...]`: it cannot be built
directly, only inherited. This keeps the shared ESP32 recipe in one place.

## How an environment is assembled

Each environment combines four mechanisms:

1. **`extends`** — inherit another section wholesale, then override. Most ESP32
   envs are just `extends = esp32_webui_base`. Inheritance can chain: the OTA
   variant does `extends = env:esp32devkitc_breadboard` to reuse the USB env and
   only swaps the upload protocol.

2. **`build_src_filter`** — pick exactly one `main.cpp`. Every env excludes all
   sources, then re-adds a single per-environment directory:
   ```ini
   build_src_filter = -<*> +<../configurations/<env>/>
   ```
   So the sources that actually compile come from `configurations/<env>/`, not a
   shared `src/main.cpp`. The `esp32_webui_base` children parameterise this with
   `custom_cfg` (default `${PIOENV}`) so a variant env can reuse another env's
   config directory without duplicating the filter — that is how the `_ota`
   variant reuses `configurations/esp32devkitc_breadboard/`.

3. **`-include configurations/<env>/config.h`** — the per-environment
   compile-time configuration (feature `#define`s, pin maps) is force-included
   into every translation unit via a `build_flags` entry. Nothing has to `#include`
   it explicitly; it is always present. See
   [configuration-flags.md](../advanced/configuration-flags.md) for the flags themselves.

4. **`lib_deps`** — the third-party libraries, usually `${libs_esp32.lib_deps}`,
   occasionally a hand-trimmed subset for a proof-of-concept env.

## The global `[env]` and the `extra_scripts` merge rule

The global `[env]` section applies to *every* environment:

```ini
[env]
build_flags  = -DPIOENV_NAME='"${PIOENV}"'          ; env name → firmware string (About panel)
extra_scripts = pre:lib/MrJ-RailwayFX.local/tools/build_embedded_data.py
    pre:scripts/name_firmware.py
```

Two of the pre-build hooks are global: the embedded-data generator (needed by all
targets) and `name_firmware.py`, which sets `PROGNAME` to the env name so each
board's binary is self-identifying (`esp32devkitc_breadboard.bin`, not
`firmware.bin`) — this prevents flashing the wrong `.bin`, a real footgun for OTA.

The trap: **PlatformIO replaces `extra_scripts`, it does not merge it.** An env
(or template) that sets its own `extra_scripts` silently drops the global ones.
`[esp32_webui_base]` handles this by re-injecting the inherited list before adding
the WebUI-only generators:

```ini
extra_scripts =
    ${env.extra_scripts}                                       ; keep the global hooks
    pre:lib/MrJ-RailwayFX.local/tools/build_webui.py           ; WebUI bundle
    pre:lib/MrJ-RailwayFX.local/tools/gen_build_info.py        ; lib-deps table
```

So `build_webui.py` and `gen_build_info.py` run only for WebUI environments,
while `build_embedded_data.py` and `name_firmware.py` run everywhere. What each
hook does is covered in [build-pipeline.md](build-pipeline.md).

## Where to look

- Add a board → a new `[env:...]` that `extends` the right template and points
  `build_src_filter` / `custom_cfg` at its `configurations/<env>/`.
- Change what compiles → `build_src_filter` and the `-include …/config.h` flag.
- A generated header didn't refresh → confirm the env inherited the right
  `extra_scripts` (the merge rule above), then see [build-pipeline.md](build-pipeline.md).
