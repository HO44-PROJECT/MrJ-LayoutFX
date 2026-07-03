# About MrJ-RailwayFX

MrJ-RailwayFX is an Arduino/ESP32 library that turns a microcontroller into a
**model-railway lighting & signalling controller**. You describe your layout
(boards, buses, devices) in a JSON file; the firmware brings it to life and
exposes a **web control panel** to drive every device — no recompilation needed
to add or change a device.

## Goal

Make realistic, layered railway effects (signals, traffic lights, animated
lamps, servo-driven mechanisms, sound) **declarative and reconfigurable**:

- **Describe, don't code.** Devices and wiring live in `config.json`, edited from
  the WebUI. Changing a pin or adding a signal does **not** require a rebuild.
- **One firmware, many layouts.** Hardware features are selected by compile flags
  in `config.h` (see [configuration-flags.md](configuration-flags.md)); the actual
  devices are runtime config.
- **Control from anywhere.** A self-contained WebUI (served from the device)
  gives a cockpit, a configuration editor, diagnostics and OTA updates.

## What it can drive

- **Signals** — Deutsche Bahn block / entry / exit signals (HP0/HP1/HP2/Sh1),
  multi-aspect, charlieplexed.
- **Traffic lights** — 3- and 4-phase road signals with POV fade transitions.
- **Lamps & effects** — static lamps, train head-lamps, beacons, plus animated
  effects: campfire, gas lamp, torch, storm…
- **Servos** — Lobot/LX-16A serial servos and PCA9685 (I²C) servo boards.
- **Audio** — DFPlayer-style serial audio modules.
- **DCC** — drive devices from an NMRA DCC command station.

## Buses (how devices connect)

| Bus | Use |
|---|---|
| GPIO | Direct microcontroller pins (LEDs, signals, traffic lights). |
| SPI (74HC595) | Chained shift-registers — many extra digital outputs. |
| I²C (PCA9685) | Servo driver boards and other I²C peripherals. |
| UART | Serial servos (LX-16A) and audio modules. |
| DCC | NMRA DCC decoder input. |
| `uart0` (log) | The serial console itself, modelled as a removable bus. |

## High-level architecture

```
 config.h  ──► compile-time feature flags (which buses/features are built in)
 config.json ─► runtime layout (boards, buses, devices) on LittleFS
 WebUI  ◄────► /api/* on the device  ◄────►  DeviceFactory / drivers / buses
```

- **`config.h`** decides *what the firmware is capable of* (WiFi, WebUI, SPI, I²C,
  DCC, OLED, logging…). Compiled in.
- **`config.json`** decides *what is actually wired up* (boards, buses, devices).
  Loaded at boot, editable live from the WebUI.
- The **WebUI** (a single bundled page served by the device) is the control and
  configuration surface.

## Platforms

- **ESP32** — full feature set (WiFi, WebUI, OTA, all buses, OLED).
- **AVR (e.g. Arduino Nano)** — a reduced subset (GPIO effects, lightweight
  status OLED), no networking.

## Where to go next

- [setup.md](setup.md) — build your first firmware.
- [configuration-flags.md](configuration-flags.md) — every `config.h` flag.
- [usage.md](usage.md) — day-to-day operation from the WebUI.
