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
- `audio/` — DFRobot DFR1173 serial MP3 (`DfRobotSerialMP3`).
- `web/` — the WebUI (`app-*.js`, i18n, styles), bundled into the firmware.

## Config-driven model
- **`config.json`** (LittleFS): three sections — `buses`, `boards`, `devices`. Editable live from the
  WebUI; the firmware only rewrites it on an explicit save (atomic — see below).
- **`DeviceFactory`** parses it (ArduinoJson) into **statically-allocated** `Device*` (fixed-size
  arrays → no heap fragmentation, predictable footprint), wiring each device to its board/bus + pins.
- **Catalog** (`device_types.json`, `board_types.json`, `bus_types.json`, `i2c_known.json`): the
  metadata the WebUI needs to render and validate. Embedded **gzipped in PROGMEM** and served from
  the firmware → a catalog change needs a **reflash**, not just a filesystem upload.

### Buses
The `buses` section replaces what used to be a single `system` section. Each bus has a free-form
JSON key, a `type`, and type-specific properties — the types themselves are library constants:

| type               | direction  | properties                    |
|--------------------|------------|--------------------------------|
| `dcc`              | input      | `pin`                          |
| `spi_master_only`  | output     | `mosi`, `sclk`, `latch`        |
| `spi_full_duplex`  | bidir      | `mosi`, `miso`, `sclk`, `cs`    |
| `uart`             | bidir      | `tx`, `rx`, `baud`             |
| `i2c`              | bidir      | `sda`, `scl`                   |

The `dcc` bus is input-only and configures the DCC receiver pin — no board attaches to it; a
device's DCC `address` is a control-mechanism attribute, independent of its physical output wiring.

```json
"buses": {
  "dcc":  { "type": "dcc", "pin": 34 },
  "uart2": { "type": "uart", "tx": 17, "rx": 16, "baud": 115200 },
  "spi":  { "type": "spi_master_only", "mosi": 23, "sclk": 18, "latch": 5 },
  "i2c0": { "type": "i2c", "sda": 21, "scl": 22 }
}
```

### Board types
A board is a physical entity exposing wiring points (pins, SPI output bits, servo IDs, …). Known
board types (also library constants), with the bus they expect and what their `wiring` field means:

| type           | expected bus       | `wiring` semantics                  |
|----------------|--------------------|--------------------------------------|
| `ESP32DevkitC` | none (root board)  | GPIO number                         |
| `HC595`        | `spi_master_only`  | output bit number (1-based)         |
| `LobotChain`   | `uart`             | servo ID within the chain           |
| `SSD1306`      | `i2c`              | — (no wiring, single device)        |

The root board (e.g. `ESP32DevkitC`) is the central MCU — it references no bus since it's the
master of all buses, but must still be declared explicitly in `boards` so the WebUI can render it.

```json
"boards": [
  { "id": "esp32",     "type": "ESP32DevkitC" },
  { "id": "spi1",      "type": "HC595",      "bus": "spi",   "pin_count": 16 },
  { "id": "servo_bus", "type": "LobotChain", "bus": "uart2" },
  { "id": "oled",      "type": "SSD1306",    "bus": "i2c0"  }
]
```

For an SPI daisy-chain, declaration order in `boards` sets the chain rank (1-based) — a board's
SPI address *is* its rank.

### Devices
Each device matches a predefined type (a C++ class), attaches to a board via `board`, and carries a
`wiring` value (scalar or array) whose meaning depends on the board type (GPIO number for
`ESP32DevkitC`, output bit for `HC595`, servo ID for `LobotChain`, …). A device may also carry a DCC
`address` (control mechanism, independent of wiring) and may be multi-state (always an OFF state
plus one or more active states). A device with no explicit `board` wires directly to the root board.

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
