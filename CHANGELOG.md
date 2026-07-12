# Changelog

Notable changes to MrJ-RailwayFX, newest first. Each entry links the backlog
issue it closes; the date is the issue's GitHub closing date. Started
2026-07-11 by reconstructing dates from `gh issue list --state closed` —
earlier project history (pre-#3) lives only in `git log`.

## 2026-07-12

- Config upload: the WebUI text wrongly implied the ESP32 reboots automatically
  right after sending a new JSON file. The 3-step flow itself (Send → Activate
  → Apply) was already correct and unchanged — only the misleading copy was
  fixed, across all 4 languages. (#53)
- RailwayCrossingLights: fixed a `-Wmaybe-uninitialized` warning on ESP32 —
  `onUs0`/`onUs1`/`onUs` are now member variables instead of locals, since a
  local declared before a `COROUTINE_DELAY_MICROS()` yield isn't guaranteed to
  survive it. (#109)
- LobotServo: fixed two compiler warnings — an explicit `IDLE` case in the
  receive-state switch (`-Wswitch`), and a `virtual` destructor gated to
  ESP32 only, where `DeviceFactory::fullReset()` deletes instances through a
  base `Coroutine*` pointer (`-Wdelete-non-virtual-dtor`). Nano/AVR build is
  unaffected. (#107)
- OilLamp: the flicker phase no longer strobes — intensity now glides toward
  a periodically-redrawn target every PWM cycle (exponential smoothing),
  instead of jumping straight to a new independently-drawn value every
  150ms. (#80)
- RailwayCrossingLights: stopping the effect no longer flashes both LEDs
  full-bright before fading out — the fade now starts from each pin's own
  actual brightness left by the flashing phase instead of resetting both to
  full intensity. (#81)
- Device editor: multi-pin wiring dropdowns (e.g. a 2/3/4-wire signal) no
  longer offer a pin already picked in another wiring slot of the same
  device — each slot's dropdown is re-filtered live against its siblings'
  current values. (#36)
- About page: gauge fill now shows green when healthy, not just orange/red —
  the warning/critical thresholds already existed, only the healthy-state
  colour was missing. (#60)
- Browser tab title now reflects the active configuration's name on the very
  first cockpit load, not only after navigating to the Boards/Buses tab.
  (#75)
- `/api/devices` now streams the JSON response one device at a time (HTTP
  chunked transfer) instead of building the whole array in one growing
  `String` — keeps RAM use flat as device count grows. (#31)
- Device and board types now have a human-readable `label` (short, shown once
  placed) and an optional `dropdownLabel` (longer, shown only in the
  selection dropdown — carries author credit for the custom PCBs and the
  MrJDB signal family without repeating it on every card). Also fixed a
  double-scrollbar in the device editor's type picker and unified the 3 MrJDB
  signals' red wire-aspect color to `#e60000`. (#77)
- Device editor: fixed the double-scrollbar in the type-picker panel (the
  panel's own list plus the modal body) — same root cause as above, tracked
  separately. (#73)

## 2026-07-10

- DCC diagnostics: live per-category packet activity pills on the
  Diagnostics tab. (#76)
- OLED: fixed the TrainHeadLamp icon mismatch and added event display for
  DCC-triggered effects. (#46)

## 2026-07-05

- Main-board edit now only offers main-board types. (#72)
- Boot log truncation fixed: UART0 was closed after `initAll()` already drove
  GPIO1/3, not before. (#71)
- Board view pin labels now follow the runtime role (TXD0/RXD0 only while
  uart0 is active, else GPIO 1/3). (#70)
- Wizard: the `$schema` meta-key was offered as a selectable board type, and
  config save failed from safe mode. (#69)
- CRASH boot-loop fixed: orphaned devices after a bus deletion no longer fall
  back to raw GPIO onto the SPI-flash pins. (#68)
- Browser tab title now reflects the active configuration (brand + layout
  name). (#67)
- GPIO1/3 are now guarded against effect/uart0 contention: conflicting
  devices are skipped (not deleted), with a UI warning. (#66)

## 2026-07-04

- Deleting the uart0 log bus no longer leaves a stale UI (bus still shown)
  or holds GPIO1/3 until reboot. (#65)
- OLED state-label table is now generated from `device_types.json`, killing
  the hand-written `_stateName` switch. (#64)
- OLED: servo speed/reverse actions now show an event, matching other
  effects. (#63)
- Serial can be re-opened at runtime when the uart0 log bus is re-added —
  counterpart of #65, no reboot required. (#18)

## 2026-07-03

- Bus tab: added a per-type explanatory description on every bus card
  (DCC/UART/I2C/SPI). (#62)
- Bus editor: the suggested key no longer sticks to `i2c` — it follows the
  selected bus type (and is valid for uart). (#61)
- Global rebrand: MrJRailwayFX → LayoutFX. (#59)
- Rebranded the HC595x2_biface board visual as the SPI Child v1.0 daughter
  card (upstream + daisy-chain connectors). (#58)
- Config: fixed the 'active' indicator diverging from the running
  `config.json` after WebUI edits. (#55)
- Removed the user-facing `pin_count` override — the output count is
  structural to the board type. (#54)

## 2026-07-02

- Made SPI card structures zero-footprint when `SPI_CARDS` is disabled. (#50)
- Decoupled the 74HC595 (SPI) output refresh from the coroutine step. (#49)
- Fixed a major visual glitch on effects. (#48)
- Consolidated all generated headers under `include/generated/`. (#45)
- Test button for light effects now available in the editor (SPI boards
  too). (#35)
- [P1] Atomic config write (`.tmp` + rename). (#29)
- [P0] Widened `STATE_TYPE` (`int8_t`) / pinned the `TEST_STATE` value. (#28)
- [P0] Concurrency: guarded the device list + hot-reload (bi-core). (#27)
- Fixed the identify ↔ nominal effect bug. (#15)

## 2026-07-01

- Icon now shown inline in the device-type picker (custom dropdown). (#33)
- ALL OFF now also stops a running pin test (identify). (#25)
- Pinout view: GPIO ↔ DCC address is now toggleable. (#16)
- SAFE MODE is now shown on the OLED. (#14)

## 2026-06-29

- Indicated linked pins of a multi-pin device. (#6)
- Pin count + icon now shown in the device-type picker. (#5)
- Device-type picker is now filtered by board capability. (#4)
- Meaningful aspect labels on cockpit buttons. (#3)
