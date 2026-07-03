# Usage

Everything is driven from the WebUI at `http://<ip>/ui`. It has three areas:
**Cockpit** (control), **Config** (declare hardware), and **About** (status,
features, OTA).

## Cockpit — controlling devices

Each device is a card. The control adapts to the device type:

- **On/off devices** (lamps, beacons, effects) — a single toggle.
- **Signals** — one button per aspect (e.g. OFF / HP0 / HP1 / HP2).
- **Traffic lights** — OFF / GO / FLASH / STOP.
- **Servos** — position / angle controls.

The small **status dot** (top-right of a card) shows the device's meta-state by
**shape**, so it reads regardless of colour:

| Dot | Meaning |
|---|---|
| hollow ring | OFF |
| spinning ring | transitioning (POV fade in progress) |
| solid disc | stable / active (the colour is a secondary cue) |

Buttons stay clickable during a transition — the firmware is interruptible and
converges to your last request.

The cockpit auto-refreshes on an interval (configurable in **Params**) so it also
reflects changes coming from DCC, physical buttons or another browser. It is also
the heartbeat that detects the device going offline.

## Config — declaring your layout

The **Config** tab has sub-tabs:

- **Boards** — a visual pinout of each board. Click a free pin to add a device on
  it; click a device's pencil to edit, or its LED button to identify the pin.
- **Buses** — add/remove buses (SPI, UART2, I²C). The `uart0 (log)` bus controls
  the serial console: remove it to free **GPIO1/3** for use as effect outputs;
  add it back to re-enable operational logs.
- **Devices** — the device editor (also reachable from a pin).

### Wiring assistant (signals & traffic lights)

Multi-pin charlieplexed devices show a per-pin **Test** button plus aspect radios.
Test each wire, tick the colour/aspect you saw, and on save the pins are
**reordered** into the canonical order automatically. On edit, the radios are
pre-checked to reflect the saved wiring.

### Default state

A device's **default state** (applied at boot) can be any of its states — e.g. a
traffic light can boot to STOP, a signal to HP0.

## About — status, features, OTA

- **Runtime** — chip, heap, filesystem, temperature, WiFi.
- **Build** — firmware version, build date, environment.
- **Features** — one badge per `config.h` flag that is *compiled in* ("built").
  Green = built into this firmware; it does **not** mean the feature is currently
  active (e.g. the `uart0` log bus may be removed while the *Log série* badge stays
  green). See [configuration-flags.md](configuration-flags.md).
- **OTA upload** — when `OTA` is built, pick a `.bin` and upload it from the
  browser; the device reboots into the new firmware. From a dev machine you can
  also `pio run -e <env> -t upload` over WiFi (espota).

## I²C scan

When the I²C bus is up (`I2C_CARDS`, `I2C_SCAN` or `OLED`), the diagnostics expose
**Scan I²C** — it walks the bus and lists responding addresses, handy to confirm
PCA9685 boards or an OLED are wired and addressed correctly.

## Recovery — safe mode

A broken or lockout config is recoverable without a reflash: **reset twice quickly**
(or power-cycle twice). The device boots ignoring the config and starts a SoftAP,
so the WebUI is reachable to repair or delete `config.json`. The change is for that
session only — the next normal boot loads the config again.

## Tip — after re-flashing

The WebUI catalogs and badges are embedded in the firmware and fetched once per
page load. After flashing, **hard-reload** the page (or use a private window) so
the browser picks up the new firmware's WebUI.
