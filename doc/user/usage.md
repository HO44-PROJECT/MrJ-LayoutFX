# Usage

Everything is driven from the WebUI at `http://<ip>/ui`. The header has a burger
menu (☰) with four views — **Cockpit**, **Vue plan** (map), **Configuration**,
**About** — plus 6 theme dots (night/amber/signal/grey/dark/light, persisted).

## Cockpit — controlling devices

A toolbar (once devices exist) offers a **search box** (filters by device id) and
one **type pill** per device category present (click to filter to that type only,
click again to clear).

Each device is a card, grouped by type with a per-group header:
- **Group ON** / **Group OFF** — turns every device of that type on/off at once
  (hidden for read-only/`static` types).

The card itself adapts to the device kind:

| Device kind | Controls |
|---|---|
| Lamp / beacon / effect / audio | One toggle button: green **ON** it turns the device on, red **ON** (showing the device is currently on) turns it off. Grey = mid-transition (still clickable — see below). Dark blue + disabled = read-only (`static`). |
| Traffic light | Four buttons: **OFF** (grey) / **GO** (green) / **CAUTION** (orange, flashing) / **STOP** (red). The active aspect gets a white glow ring. |
| Railway signal | One button per aspect declared for that signal type (e.g. HP0/HP1/HP2), labelled "code · meaning" when a known meaning exists (HP0 = stop, HP1 = clear, HP2 = slow, HP0+Sh1 = shunting). |
| UART servo | Speed presets **STOP / SLOW / MID / FAST** + **REV** (reverses rotation direction). |
| I²C continuous motor (PCA9685Motor) | **STOP** + one button per configured state (custom label). Buttons are disabled (not just inert) while the device is transitioning. |
| I²C positional servo (PCA9685Servo) | **STOP** + one button per configured position (custom label). Same disabled-while-busy behaviour. |

The small **status dot** (top-right of a card) shows the device's meta-state by
**shape**, so it reads regardless of colour:

| Dot | Meaning |
|---|---|
| hollow ring | OFF |
| spinning ring | transitioning (POV fade or startup delay in progress) |
| solid disc | stable / active (the colour is a secondary cue) |

For most device kinds, buttons stay clickable during a transition — the firmware
is interruptible and converges to your last request. Every device's **On/Off
transition also honours its configured startup delay** (`start_delay_ms` /
`start_delay_random_ms`), the same way whether you use this per-device button, a
group button, or the global All ON/OFF below.

Each card also shows a meta strip: DCC address (if any) and board/pin location
("Carte N · pin X,Y", "GPIO X,Y", or "Carte N · servo ID").

The header's **All ON** / **All OFF** buttons (green/red, visible on the Cockpit
view only) act on every device at once. The cockpit auto-refreshes on an interval
(configurable in Configuration → Interface) so it also reflects changes coming
from DCC, physical buttons, or another browser tab — this polling loop is also
the heartbeat that detects the device going offline (shown as a reconnect banner).

If no devices are configured yet, the empty state offers **Configurer** (jumps to
Config → Boards) and **Assistant de configuration** (opens the setup wizard).

## Configuration — declaring your layout

Sub-tabs: **Cartes & extensions** (Boards), **Fichiers config** (Files), **Bus**
(Buses), **Diagnostics**, **Interface**.

The layout name field at the top auto-saves as you type and updates the browser
tab title. If a different config file is pending activation, a **dirty banner**
appears with an **Appliquer** button (activates the file, reloads the firmware
config, no reboot needed).

### Boards & extensions

Toolbar: **+ Carte** (add a board), **Actualiser** (refresh from the firmware),
a 3-way **GPIO / DCC / DÉLAI** pin-label toggle (see below), **⬇ main.cpp**
(dev-server only), **Assistant** (re-open the setup wizard — warns before
overwriting an existing config), **Réinitialiser** (wipe all buses/devices back
to a single default board, with confirmation).

Each board is a card with **ALLUMER TOUT** / **ÉTEINDRE TOUT** (all devices on
that board), a pencil **Edit** button, and (for boards you added yourself, not
the built-in MCU board) **Supprimer**.

Below that, the board's pins are drawn as a small diagram, one cell per pin.
A cell's colour tells you its role at a glance:

| Look | Meaning |
|---|---|
| plain/default | free output, no device — click to blink-identify it |
| green fill | device assigned, currently ON |
| red fill | device assigned, currently OFF |
| dashed lavender | device saved in config but the firmware hasn't reloaded yet ("cfg-only") |
| blue fill | free pin currently under test (`/api/test/gpio` or `/api/test/spi`) |
| pale blue, "reserved" label | GPIO used by a configured bus (SDA/SCL/MOSI/TX…) |
| tan, "STRAP" | ESP32 strapping pin — avoid unless you know what you're doing |
| grey-blue | input-only GPIO |
| khaki, "JTAG" | JTAG-reserved pin |
| no border, blank | GND, power, or non-connectable pin |

Each pin cell that can carry a device shows up to three small overlays:
- **Pencil (top-left)** — opens the device editor: create a new device on a free
  pin, or edit the one already there.
- **Lamp icon (bottom-right)** — the **identify** button: blinks the physical
  LED/output on that exact pin (triple-blink) so you can match the WebUI to the
  real wire. Green tint = this is the pin currently blinking; red tint =
  idle/clickable. Only one pin identifies at a time.
- **State badge (top-right)**, multi-state devices only — the current
  position/aspect index (e.g. "P2" for an I²C servo's position 2, or a bare
  number for a signal aspect).

Clicking the pin cell itself (not the icons) acts on the device: a binary device
toggles on/off instantly (no startup delay — this is a hardware wiring check,
not cockpit control); a multi-state device (positional servo, signal, etc.)
cycles to its next state, also instantly. Clicking a free pin starts/stops the
identify blink, same as its lamp icon. Hovering any pin of a multi-pin device
highlights every pin it uses; clicking pins that highlight for a few seconds.

On SPI/I²C expansion boards, device rows are listed flatly instead of as a pin
diagram (ON/OFF toggle, servo speed presets where relevant, pencil edit, ✕
delete, **+ Ajouter** to add another device on that bus).

#### GPIO / DCC / DELAY pin-label modes

The 3-way toggle changes what each pin cell's number/label shows — the toggle
only changes pins that already carry a device; empty pins always show their
plain GPIO number or bus channel name.

| Mode | Pin with a device | What's shown |
|---|---|---|
| **GPIO** (default) | any | the GPIO number, or the bus channel name on SPI/I²C boards (e.g. "Q3", "CH7") |
| **DCC** | has a DCC address configured | `@<address>` (e.g. `@43`) |
| **DCC** | no DCC address configured | `@-` |
| **DÉLAI** (delay) | has a startup delay configured | the delay, e.g. `4.0s+2s` (fixed + random upper bound) or `500ms` |
| **DÉLAI** (delay) | no startup delay configured | `0s` |

Your choice is remembered across page reloads.

### Wiring assistant (signals & traffic lights)

Multi-pin charlieplexed devices (DB block/entry/exit signals, traffic lights)
show a per-pin **Test** button plus aspect radio buttons in the device editor.
Test each wire (it blinks, holding the others low), tick the aspect/colour you
actually saw on that wire, and on save the pins are **reordered** into the
canonical order automatically — no need to know the "right" wiring order ahead
of time. On edit, the radios are pre-checked to reflect the saved wiring.

### Default state

A device's **default state** (applied at boot, in the device editor) can be any
of its states — e.g. a traffic light can boot to STOP, a signal to HP0. This is
independent of its live/runtime state shown elsewhere in the UI.

### Buses

**+ Bus** adds a new bus (I²C, SPI, UART, or DCC, depending on what the firmware
build supports). Suggestion cards appear automatically for buses the firmware
supports but that aren't configured yet (e.g. "add the uart0 log bus", "add the
DCC bus") — one click enables them. Configured buses show **Modifier** (edit
pins/parameters) and **Supprimer** (remove — with a warning listing any devices
that would become dormant, since removing a bus never deletes devices, only
their board/wiring).

The `uart0 (log)` bus controls the serial console: remove it to free **GPIO1/3**
for use as effect outputs; add it back to re-enable operational logs. Its pins
are fixed, so it has no Edit button.

### Files

Manage saved config files: **Snapshot** (timestamped copy), **⬇ Télécharger**
(download the JSON), **Choisir** (mark a different file to activate on next
Apply/restart — shows the dirty banner), **Renommer**, **✕ Supprimer** (not
available for the currently active file). A file-upload control at the bottom
lets you upload a `.json` config from your computer (validated client-side
before sending).

### Diagnostics

- **Scan I2C** — walks the I²C bus and lists responding addresses with known
  chip names where recognised (e.g. a PCA9685 board or an OLED). Only shown
  when an I²C-capable feature is built in.
- **DCC activity** (when a DCC bus is configured) — one pill per message
  category (raw/bus, speed, function, accessory, signal). A pill's dot flashes
  briefly on every fresh packet of that kind — lets you tell "bus dead" (nothing
  lights up) from "bus alive but this decoder ignores this category" (only some
  pills light up) at a glance. Click a pill to filter the event log below it to
  that category (its background highlights); click again to return to the
  unfiltered view. The log lists decoded events (time, category, address, value,
  repeat count, matched device if any) newest first; identical repeated packets
  (e.g. a throttle held steady) collapse into one growing `×N` line instead of
  flooding the log.

### Interface

- **Language** — FR / EN / DE / ES, switches the whole UI immediately.
- **Poll interval** — how often the Cockpit/Boards views refresh from the
  device (500 ms–30 s, default matches the firmware's typical response time).
  Lower = more responsive but more network/CPU load.

## About — status, features, OTA

- **Runtime** — chip, heap, filesystem, temperature, WiFi signal, with colour
  thresholds (amber past 65% usage, red past 85%).
- **Build** — firmware version, build date, environment.
- **Features** — one badge per `config.h` flag that is *compiled in* ("built").
  Green = built into this firmware; it does **not** mean the feature is
  currently active (e.g. the `uart0` log bus may be removed while the *Log
  série* badge stays green). Hover a badge for its full description. See
  [configuration-flags.md](../advanced/configuration-flags.md).
- **OTA upload** — when `OTA` is built, pick a `.bin` and click **Envoyer** to
  upload it from the browser, with a live progress bar; the device reboots into
  the new firmware and the page auto-reloads a few seconds later. From a dev
  machine you can also `pio run -e <env> -t upload` over WiFi (espota).

## Setup wizard

Runs automatically on first boot (no devices configured yet) or on demand via
**Assistant** on the Boards page (which warns before overwriting an existing
config). Steps: pick a language, name your main board, enable the buses you
need (I²C/SPI/UART, each with its own pin fields), add any expansion boards on
those buses (type, ID, I²C address — auto-suggested and collision-checked), then
review a summary before **Terminer** writes the whole config and reloads the
firmware.

## Editors (device / board / bus)

Opened from a pin, a board card, or the Buses tab. All three share the same
shape: a form with **Annuler** / **Sauvegarder** (and **Supprimer** when editing
something that already exists), Escape to cancel and Enter to save as keyboard
shortcuts, and live validation (duplicate IDs, out-of-range values, missing
required wiring) before the save is accepted.

The **device editor** additionally has: a searchable type picker (filtered to
what's valid for the board/bus you opened it from), per-wire **Test** buttons on
GPIO/SPI devices (blinks that exact wire), the wiring-assistant aspect radios
for charlieplexed signal/traffic-light types, DCC address, default state,
startup delay (fixed + random), and type-specific extras (position list for I²C
servos, state list for I²C motors, PWM pulse-width calibration).

## Recovery — safe mode

A broken or lockout config is recoverable without a reflash: **reset twice
quickly** (or power-cycle twice). The device boots ignoring the config and
starts a SoftAP, so the WebUI is reachable to repair or delete `config.json`.
The change is for that session only — the next normal boot loads the config
again.

## Tip — after re-flashing

The WebUI catalogs and badges are embedded in the firmware and fetched once per
page load. After flashing, **hard-reload** the page (or use a private window) so
the browser picks up the new firmware's WebUI.
