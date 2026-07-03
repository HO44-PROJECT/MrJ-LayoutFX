# The pre-build code-generation pipeline

Several kinds of "source" in this project are not C++: the device/board/bus
catalogs are JSON, the WebUI is HTML/CSS/JS, and the declared library versions
live in `platformio.ini`. None of that can be `#include`d directly. Three
pre-build hooks bridge the gap: they run before compilation and emit C++ headers
into a single generated directory, which the firmware then includes normally.

```
data/*.json ─┐
             ├─ build_embedded_data.py ─→ include/generated/embedded_*.h
board_types  ┘                            include/generated/embedded_board_pincounts.h

src/web/*   ──── build_webui.py       ─→ include/generated/webui_html.h

platformio.ini
 (lib_deps)  ─── gen_build_info.py    ─→ include/generated/build_info.h
```

All generated headers land under `lib/MrJ-RailwayFX.local/include/generated/`,
which is **gitignored**. They are regenerated on every build, never hand-edited,
and never committed — the source of truth is always the JSON / web / `.ini` input.
Each generator also creates the directory on demand, so a clean checkout builds
without any manual setup.

## `build_embedded_data.py` — JSON catalogs → PROGMEM

Runs for **every** environment (wired from the global `[env] extra_scripts`).
It reads the four structural catalogs from `data/` and emits, for each, a gzipped
byte array in PROGMEM:

| Source (`data/`) | Header (`include/generated/`) | Symbol |
| --- | --- | --- |
| `board_types.json`  | `embedded_board_types.h`  | `BOARD_TYPES_GZ` |
| `device_types.json` | `embedded_device_types.h` | `DEVICE_TYPES_GZ` |
| `bus_types.json`    | `embedded_bus_types.h`    | `BUS_TYPES_GZ` |
| `i2c_known.json`    | `embedded_i2c_known.h`    | `I2C_KNOWN_GZ` |

Each catalog is minified, gzip-compressed, and served straight from flash by the
API with `Content-Encoding: gzip` — the browser inflates it. Keeping the catalogs
in PROGMEM instead of LittleFS is a deliberate memory/partition choice (see
[`../architecture/memory.md`](../architecture/memory.md)).

The script also emits one **uncompressed** header, `embedded_board_pincounts.h`:
a compact `type → pin_count` table built from the `spi_master_only` entries of
`board_types.json`. The firmware needs those pin counts at boot to size 74HC595
daisy-chains when a board config omits `pin_count`, and it can't inflate the
gzipped catalog on the AVR — so this tiny table is passed to
`DeviceFactory::load()` directly. (See its use in `ConfigManager::init`.)

This hook also runs standalone for a quick regen without a full build:

```sh
python3 lib/MrJ-RailwayFX.local/tools/build_embedded_data.py
```

## `build_webui.py` — web sources → one gzipped page

Runs only for WebUI environments (added by `esp32_webui_base`). It assembles the
entire single-page UI from `src/web/` into one HTML document, minifies it, gzips
it, and emits `webui_html.h` (`WEBUI_HTML_GZ` / `WEBUI_HTML_GZ_LEN`), which
`WebUI.cpp` serves with `send_P`.

The HTML skeleton `webui.html` has four markers filled in at build time:

| Marker | Filled from |
| --- | --- |
| `%%STYLE%%` | `style.css` (minified with `rcssmin`) |
| `%%I18N%%`  | `i18n.js` — the fr/de/es/en translation tables |
| `%%ICONS%%` | `icons.js` — the SVG icon map |
| `%%APP%%`   | the `app-*.js` modules concatenated in order, then minified |

The JS is split into modules (`app-pure`, `app-core`, `app-wizard`, `app-config`,
`app-boards`, `app-about`, `app-device-editor`, `app-board-editor`) purely for
authoring; they are bundled as a single unit. Edit the split files in
`src/web/`, never the generated header. The minifiers (`rjsmin`, `rcssmin`) are
`pip install`ed automatically on first build if missing.

## `gen_build_info.py` — resolved lib deps → About panel

Runs only for WebUI environments. It emits `build_info.h`, a `kLibDeps` table of
`{name, version-constraint}` pairs that `DeviceStatusApi` exposes at
`/api/status` and the UI shows in the "Libraries" card.

Crucially, it reads the dependency list from PlatformIO's *resolved* project
options — `env.GetProjectOption("lib_deps")` — not by parsing `platformio.ini`.
PlatformIO has already applied `extends`, expanded `${...}`, and merged the global
`[env]` by then, so the table reflects exactly what the active environment builds
against and cannot drift from the real configuration.

## Brand substitution — one constant for the whole project

The displayed brand name has a **single source of truth**:
`LFX_PROJECT_NAME` in `include/LayoutFX_default.h`. Firmware display
strings (OLED screens, the AP-SSID default, API messages) use the macro
directly. The web sources and the JSON catalogs carry a `%%BRAND%%` token
instead of a literal name: `build_webui.py` substitutes it across the whole
assembled bundle (HTML **and** bundled JS, so i18n labels are covered), and
`build_embedded_data.py` substitutes it in each minified catalog before
gzipping. Both scripts parse the same `#define`. Rebranding the display name is
therefore a **one-line edit** — internal identifiers (type strings, macro
prefixes, class names) are deliberately not covered.

## Reproducible output → minimal rebuild churn

Both `build_embedded_data.py` and `build_webui.py` gzip with `mtime=0`. Gzip
normally stamps its output with the current time, which would make every build
produce a byte-different header and force a recompile of everything that includes
it. With `mtime=0` the header changes **only when its input changes**, so an
unchanged catalog or WebUI doesn't trigger spurious rebuilds.

## What a change requires: reflash vs. filesystem upload

Because the catalogs and the WebUI are baked into the firmware image (PROGMEM),
**editing `data/*.json` or `src/web/*` requires a reflash** — the running device
won't see the change from a filesystem upload. LittleFS holds only runtime data:
the user's `config.json` (and small bookkeeping files). So:

- Changed a catalog, a device type, an icon, a translation, or any UI code
  → rebuild + **flash the firmware**.
- Changed only the device configuration (via the WebUI, or by editing
  `config.json`) → that lives in LittleFS; no reflash needed.
