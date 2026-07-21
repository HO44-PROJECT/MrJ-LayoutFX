# Configuration examples

[Docs](../README.md) / User guide / Configuration examples

Three ready-to-use `config.json` files live in
[`examples/`](../../examples/) at the repo root, from simplest to most
elaborate. They're a starting point to copy and edit, not something you're
expected to use as-is — device `id`s, pins and addresses are placeholders.

Each one validates against
[`schemas/config.schema.json`](../../schemas/config.schema.json); if you hand-edit
a copy, VS Code will flag schema errors live as you type (the `$schema` line at
the top of each file points at it).

## Loading one

- **From the WebUI** (recommended) — Configuration → Files → **Upload Config**,
  pick the file, then **Choisir** to activate it (see
  [usage.md](usage.md#files)).
- **From a source build** — copy it to `data/config.json` in your project
  before `pio run -t uploadfs` (see
  [building-from-source.md](../advanced/building-from-source.md)).

## `config_esp32mini.json` — minimal

The smallest valid config: one ESP32 Mini board, no buses, no devices yet.
This is close to what you get after flashing (an empty layout) — a good sanity
check that the schema and board type are right before you start adding
devices, or a clean base to build a plain-GPIO layout on top of.

```json
{
  "boards": [{ "id": "esp32", "type": "ESP32Mini" }],
  "buses": {},
  "devices": []
}
```

## `config_2_ext.json` — mixed buses, two SPI expansion boards

A fuller layout on one ESP32 DevKitC: DCC input, an I²C bus, a UART bus
driving a chain of serial servos, and two `HC595x2_biface` (74HC595 shift
register) expansion boards carrying most of the lamp/beacon/signal devices.
Shows:

- **Bus declarations** (`buses`) for `dcc`, `spi`, `uart2`, `i2c`, plus the
  `debug`/log UART — each bus is a named entry with its own pins.
- **Board declarations** (`boards`) referencing a bus by key (`"bus": "spi"`).
- **Devices wired directly to the MCU** (`"board": "esp32"`) alongside
  **devices wired to an expansion board** (`"board": "ext1"` / `"ext2"`) — the
  `board` field is what tells them apart, wiring numbers are just pin/channel
  indexes local to whichever board they name.
- **Multi-wire devices** (`MrJDBEntrySignal`, `MrJDBExitSignal`,
  `DoubleBeacon`) using a wiring **array** instead of a single pin number —
  see [device-types.md](device-types.md) for how many wires each type needs.
- A **serial bus servo** (`SerialServo`) addressed by its bus ID
  (`"address": 20`) rather than a GPIO pin, wired to the `servo_bus` board
  which itself sits on the `uart2` bus.

## `config_spi_sample.json` — SPI-focused

A narrower version of the same idea, trimmed down to just the SPI expansion
path plus one signal and one serial servo — useful as a shorter reference when
you only care about the 74HC595 wiring pattern (many LED-style devices behind
few GPIO pins) without the DCC/I²C noise.

## Screenshots

Not included yet — each example above would benefit from a WebUI screenshot
(Boards page pin diagram + Cockpit view) once loaded on real hardware. If
you've built one of these layouts, a PR adding screenshots under
`doc/user/img/` and linking them here would be very welcome.

## Building your own from scratch

The [setup wizard](usage.md#setup-wizard) is the easiest way to get a first
valid config without hand-writing JSON — it walks through boards, buses and
expansion boards and writes the file for you. Hand-editing is normally only
needed for bulk device declarations (many similar lamps) that are faster to
copy-paste than to click through one at a time.
