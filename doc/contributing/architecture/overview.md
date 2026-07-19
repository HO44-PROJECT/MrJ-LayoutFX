# Architecture overview

[Docs](../../README.md) / [Contributing](../README.md) / [Architecture](README.md) / Overview

A **config-driven** firmware: a JSON config describes the hardware (boards, buses, devices);
a factory instantiates device objects; each device is an **AceRoutine coroutine** that drives
its output(s). Control (WebUI / API / DCC) only sets *intent*; the coroutines do the work.

## Module layout (`src/` + `include/`)
- `config/` — `ConfigManager` (load / persist / hot-reload) and `DeviceFactory` (JSON → `Device` instances).
- `devices/` — the `Device` base class (state machine, pin handling).
- `led_fx/`, `signals/`, `traffic/`, `servo/` — the effect implementations (coroutines).
- `bus/`, `spi/` — the bus registry and the 74HC595 SPI shift-register chain.
- `oled/` — `OledDisplay` (rich display) / `StatusOled` (lightweight, AVR).
- `api/` — `ApiServer` (WiFi + HTTP) and the endpoint handlers.
- `dcc/` — NmraDcc integration and accessory-address dispatch.
- `audio/` — DFPlayer Mini.
- `web/` — the WebUI (`app-*.js`, i18n, styles), bundled into the firmware.

## Config-driven model
- **`config.json`** (LittleFS): three sections — `buses`, `boards`, `devices`. Editable live from the
  WebUI; the firmware only rewrites it on an explicit save (atomic — see below).
- **`DeviceFactory`** parses it (ArduinoJson) into **statically-allocated** `Device*` (fixed-size
  arrays → no heap fragmentation, predictable footprint), wiring each device to its board/bus + pins.
- **Catalog** (`device_types.json`, `board_types.json`, `bus_types.json`, `i2c_known.json`): the
  metadata the WebUI needs to render and validate. Embedded **gzipped in PROGMEM** and served from
  the firmware → a catalog change needs a **reflash**, not just a filesystem upload.

## Pins & buses
- A `PIN_ID` abstracts a target: a raw MCU **GPIO**, or an **SPI** 74HC595 (card, channel). Devices
  drive their outputs only through this.
- Bus kinds: **GPIO** (direct), **SPI** 74HC595 chain (a per-loop buffer flush), **I2C** (PCA9685
  servo / motor), **UART** (Lobot serial servo, DFPlayer audio).
- The output is written by the owning device's coroutine only — one owner per pin (see
  [concurrency.md](concurrency.md)).

## Effects = coroutines
Each device subclasses `Device` and implements `runCoroutine()`. `newState()` records the target
(`desiredState`); the coroutine converges (`INIT → transit/busy → stable`). `newState` is the single
control entry point — details and the state sentinels are in [concurrency.md](concurrency.md).

## Boot sequence (defensive)
Roughly, in `LayoutFX::init()`: serial/log → safe-mode check → OLED (early, shows boot context) →
LittleFS + config load (**skipped in safe mode**) → `DeviceFactory` build + `applyDefaultStates` +
`initIdlePins` → buses → WiFi / HTTP (`ApiServer`, Core-0 task) → OTA / mDNS → DCC. Order matters:
pins are parked idle before anything drives them, and the API / DCC come up only after devices exist.

## Persistence
`config.json` lives on LittleFS. The WebUI reads it (`GET /api/config`), edits in memory, pushes it
back (`POST /api/config`), then triggers a **hot-reload** (no reboot). Writes are **atomic**
(temp file + rename) so a power cut can't leave a truncated config.

## Bi-core & concurrency
Core 0 = WiFi + HTTP; Core 1 = the AceRoutine scheduler + DCC + hot-reload. The full model (the single
`newState` concurrency point, the device-list mutex, the invariants) is in [concurrency.md](concurrency.md).

## WebUI (embedded)
`web/app-*.js` + styles + i18n (fr / de / es / en) are concatenated, minified, gzipped and compiled
into the firmware as a PROGMEM blob → a WebUI change needs a **reflash + browser hard-reload**. The
page fetches the catalog once and renders the cockpit, the config editor and the setup wizard from it.
