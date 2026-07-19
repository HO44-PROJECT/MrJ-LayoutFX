# Changelog

Notable changes to MrJ-RailwayFX, newest first. Each entry links the backlog
issue it closes; the date is the issue's GitHub closing date. Started
2026-07-11 by reconstructing dates from `gh issue list --state closed` —
earlier project history (pre-#3) lives only in `git log`.

## 2026-07-19

- Fixed: the cockpit's per-device "Turn on"/"Turn off" button (generic card)
  ignored the device's configured startup delay (`start_delay_ms`/
  `start_delay_random_ms`, #8), switching instantly instead of honouring it
  like the global ALL and group buttons already did.
  `DeviceApi::_onPostDevice()` (`/api/device`) had `skipDelay` hardcoded to
  `true` for every caller; it now reads an optional `skip_delay` field
  (same convention as `/api/all`), defaulting to `false`. The Boards tab's
  multi-state pin cycling (`dbgCycleDev`), which must stay instant for
  wiring tests, now passes `skip_delay: true` explicitly to keep its
  existing behaviour. Validated in the browser. (#128)

- Devices can now carry an optional free-text `comment` field (e.g. "quai 1",
  "montagne") for the layout author's own reference — pure metadata, no
  functional effect on firmware. Added to `config.schema.json`, the device
  editor form, and surfaced as a tooltip on the device's pin cell / bus row.
  `config.json` round-trips raw through `/api/config` (never parsed by
  firmware), so no C++ changes were needed for persistence — but three
  separate WebUI code paths that rebuild the device object for the editor
  (`openDevEditorById`'s bus/runtime/cfg-only branches in
  `app-device-editor.js`, and `mergeDeviceForEditor` in `app-pure.js`) each
  keep their own field allowlist and were silently dropping `comment` on
  reopen even though the save path worked correctly — all three now carry
  it forward. Validated in the browser. (#83)

## 2026-07-18

- The SPI 74HC595 daisy-chain now resizes on hot-reload instead of requiring
  a full reboot: changing a card's `pin_count`, or adding/removing a card,
  takes effect immediately. `Spi595Bus::init()` now only handles one-time
  hardware bring-up (`SPI.begin()`/`pinMode()`), while a new `Spi595Bus::resize()`
  recomputes chain sizing (`_totalBytes`/`_cardBitOffset`/`_cardPinCount`) and
  repaints the hardware — safe to call on every config load.
  `BusRegistry::activateSpi()` now calls `resize()` instead of no-op'ing once
  the bus is already active, and `DeviceFactory::load()` calls it even when
  the reloaded config has zero SPI cards, so removing the last card correctly
  zeroes `Spi595Bus` (`ready()` back to `false`) instead of leaving stale
  sizing and phantom outputs behind. Validated on hardware. (#56)

- Disabled feature badges in the About/Features modal no longer use
  `text-decoration: line-through` — the grey background/color/border already
  communicate "disabled" clearly, and the strikethrough hurt readability.
  Pure CSS change. (#126)

- The DCC pin is now reserved at runtime — only while a `dcc` bus is actually
  configured — instead of always being reserved at compile time
  (`DCC_PIN`), mirroring how uart0's GPIO1/3 reservation became config-driven
  (#65/#18). `sys_pins` lists the pin only when `DeviceFactory::dccPin() >= 0`,
  `/api/test/gpio` and the identify/wiring-test endpoints now reject that pin
  only while the bus is active, and `DeviceFactory` skips (rather than
  silently double-drives) any device wired to the same pin as the configured
  dcc bus — same pattern as the existing uart0 wiring-conflict guard (#66).
  Removing the `dcc` bus releases the pin **immediately**, not after a
  reboot: `DccDrivable::end()` calls `detachInterrupt()` on the pin NmraDcc's
  ISR was attached to (NmraDcc itself has no teardown API, but on ESP32 the
  pin number we pass to `NmraDcc::pin()`/`init()` is the same one
  `attachInterrupt()` uses internally, so external code can detach it without
  any library changes); re-adding the bus re-attaches cleanly. The WebUI
  wizard and bus editor previously read the compiled DCC pin straight out of
  `sys_pins` to suggest it when adding the bus — since that entry is now
  runtime-only, a new always-present `dcc_pin_default` status field
  (the compiled `DCC_PIN`) was added for them to fall back on before the bus
  exists. Validated on hardware. (#19)

## 2026-07-17

- The structural OLED (`OledDisplay`) is now config-driven instead of fixed
  at compile time: deleting the "SSD1306" board in the WebUI now actually
  blanks the screen, and re-adding it (or changing its declared
  `oled_height`, 32 or 64) re-configures the display at runtime — no
  reboot. Both SSD1306 sizes are handled by a single `U8G2` instance
  re-configured via the same `u8g2_Setup_ssd1306_i2c_*` call the compiled
  subclasses used internally (no dual-driver compilation, no more
  `#if OLED_HEIGHT` in the draw functions). `DeviceFactory::findOledBoard()`
  is the new source of truth, pushed to `OledDisplay::configure()` by
  `ConfigManager` after every init/reload/hot-reload — the actual U8G2
  mutation happens on the OLED's own Core 0 task, since `configure()` may be
  called from Core 1. Validated on hardware. (#51)
- Added 3 new UI theme variants — grey (neutral concrete), dark (low-light,
  brightened accents for readability), light (pure white, max contrast) —
  alongside the existing night/amber/signal, following the same 17-variable
  CSS custom-property structure so every theme covers the same surface.
  `setTheme()` and the theme-dot switcher updated accordingly. The "blue
  icon on the green theme" bug mentioned in the original issue could not be
  reproduced (no theme named "green" exists, no hardcoded blue color found
  in the icon set) and was dropped from scope. Validated in-browser via
  `tools/local_server.py` (no reflash needed — pure CSS/JS). (#38)
- Cockpit: the grouped "ALL ON"/"ALL OFF" header buttons and the per-type
  group "TURN ON"/"TURN OFF" buttons were still all-caps — #120 had only
  normalized the individual device-card button's OFF-state label. Now
  normal-case in all 4 languages (e.g. "Tout allumer"/"Alles ein"/"Encender
  todo"/"All on", "Allumer"/"Einschalten"/"Encender"/"Turn on"), matching
  #120's precedent. The individual device-card button's ON-state label
  ("Éteindre"/"Ausschalten"/"Apagar"/"Turn off", shown while the device is
  active) was also normal-cased to match, since it reads as a plain action
  button next to the already-fixed OFF-state label. Validated on hardware.
  (#124)
- Added tooltips to every board-toolbar and cockpit action button that had
  none, or a redundant one restating its own label (e.g. the GPIO/DCC pin-
  label toggles just showed "GPIO"/"DCC"): header ALL ON/OFF, Config→Boards
  "+ Carte"/"Actualiser" and the GPIO/DCC/DÉLAI pin-label toggles, each
  board card's All ON/All OFF/edit/Delete buttons, and the cockpit's
  per-device toggle and per-type group Turn on/off buttons. Static HTML
  buttons now support a new `data-i18n-title` attribute (mirroring the
  existing `data-i18n-placeholder` mechanism) so their `title` is set from
  i18n on load and language change; JS-rendered buttons use the existing
  inline `title="' + t('key') + '"'` pattern. 15 new i18n keys × 4
  languages. Validated on hardware. (#125)
- Fixed a startup-delay bug chain found while re-testing the cockpit path
  (follow-up to #8/#119): a grouped Turn-Off glitched (instant off, flash
  back on, then fade) because the delay macro used to block the whole
  effect coroutine instead of just the target-state flip; then, once made
  non-blocking, `DEVICE_WAIT_STATE_CHANGE()`'s `COROUTINE_AWAIT` never fell
  through to commit the switch, silently leaving devices stuck off; then,
  once fixed, effects re-invoking their own `setState()` internally (e.g.
  `GasLampDefect`'s "stay OFF" branch) kept re-arming the same deadline
  forever, spamming `[#8] delayed switch armed/fired` in a loop. Now fixed
  with a `_pendingDesiredState` guard so a delay is only (re-)armed for a
  genuinely new target, while the deadline is still committed as a side
  effect of the existing `COROUTINE_AWAIT`. The armed/fired trace is now
  always-on info (`LOG_SERIAL`), not gated behind the debug flag. Validated
  on hardware. (#121)
- Renamed the whole `DEBUG_*` macro family (`DEBUG_SERIAL`, `DEBUG_OLED`,
  `DEBUG_INIT`, `DEBUG_PRINT`, `DEBUG_PRINTLN`, `DEBUG_PRINTF`) to
  `MRJ_DEBUG_*` across every config and the library, to stop colliding with
  Adafruit BusIO's own `DEBUG_SERIAL` convention. Pure rename, no behaviour
  change. Validated on hardware. (#122)
- New `API_AUDIT` build flag (mirrors `DCC_AUDIT`): gates the 23 per-request
  `API: ...` log lines across the 4 API source files behind an opt-in
  `#define`, off by default. Added to the About page's feature badge system
  (key, JSON emission, label/group, i18n ×4). Validated on hardware. (#123)

## 2026-07-16

- About page: feature badges were already grouped by category (Core/Bus/
  Oled/Logging/Behaviour) — confirmed still in place, no code change
  needed there. Fixed while checking: disabled ("off") badges used the
  palette's palest text tone plus an extra `opacity: .65`, making them
  unreadable — now uses a darker, still-muted tone with no added opacity
  (badge stays visually distinct via its grey background + strikethrough).
  Validated on hardware. (#93)
- About page: `fmtBytes()` gained a GB tier (was capped at MB, so a value
  ≥ 1 GB would have rendered as e.g. "2048.0 MB") and `fmtUptime()` gained
  a days tier ("Xj HH:MM:SS", i18n ×4) instead of hours rolling past two
  digits. The `millis()` wraparound at ~49.7 days of continuous uptime is
  a known Arduino-core limitation, accepted as-is (a DCC layout reboots far
  more often than that in practice) — documented in code, not guarded
  against. (#116)
- Cockpit: the device card action button used a near-black background for
  both the OFF state and the busy/transitioning state — softened to a
  slate gray (#52525f) with light text. The OFF label also moved from
  all-caps "ALLUMER"/"EINSCHALTEN"/"ENCENDER"/"TURN ON" to normal-case
  "Allumage"/"Einschalten"/"Encender"/"Turn on" (fr/de/es/en); the busy
  state keeps its "…" label, only its color changed. Validated on
  hardware. (#120)
- Interface tab: the "Refresh"/"Rafraîchissement" setting is now labelled
  "Auto refresh"/"Rafraîchissement auto" (fr/de/es/en) — clearer that it
  controls the auto-refresh interval, not a manual refresh action. Also
  removed the dead legacy `feat.serial` badge constant and its orphaned
  tooltip (superseded by `log_serial`/`debug_serial`, and never actually
  populated or rendered since that split — nothing to restore). Validated
  on hardware. (#21)
- New light effect `GasLampDefect`: same ignition/flicker/brightening/
  stable-flame/extinction phases as `GasLamp`, plus a rare, brief dark
  glitch during the stable flame — a "gas lamp that occasionally
  malfunctions," distinct from `DefectLamp` (untouched, still cycles
  quickly and often by design). Registered as a full device type (factory,
  OLED icon, WebUI icon/i18n, config schema). Validated on hardware. (#111)

## 2026-07-15

- About page: the WiFi signal bar was inverted — a strong signal (less
  negative dBm) drew a long/red bar and a weak signal drew a short/green
  one. Fixed the percentage formula's polarity and moved the RSSI good/weak
  bands to -70/-80 dBm (from -60/-75), better suited to ESP32/IoT links.
  RSSI bands, the temperature bar's display range, and the shared bar
  warn/crit color thresholds are now named constants so they can't silently
  drift apart. Validated on hardware. (#113)
- Board editor: the Buses tab and its wizard "+ Add" shortcut could show a
  bus that was already active as a ghost "not yet added" suggestion. The
  check compared a board type's `linkedBuses[].key` against the config's own
  bus keys, but the wizard assigns its own default keys (`i2c0`, `uart1`,
  `spi`) independently of a board type's declared key (`i2c`, `uart2`,
  `spi`) — matching is now done on bus type + pins instead. Reproduced on a
  fresh install with I2C + Serial2 activated via the wizard; validated on
  hardware. (#114)
- SPI: the identify button did nothing until at least one device existed on
  the card. `Spi595Bus::activateSpi()` was only called when an actual device
  got parsed (`DeviceFactory::_pin()`), so a bare SPI card left the bus
  permanently un-initialised and `Spi595Bus::ready()` false. The bus now
  activates right after board parsing as soon as one SPI card is declared,
  and the identify endpoint gained the same "SPI not ready" guard the test
  endpoint already had. Validated on hardware. (#115)
- Config list: filenames were truncated right after ".js", making a `.json`
  file look like a `.js` one — `.cfg-fname` had a fixed `140px` width. Widened
  to `33ch` to fit `kFsNameMax` (32 chars incl. extension) in full, and added
  a tooltip with the full name as a safety net. Validated on hardware. (#112)
- Servo action buttons (STOP/SLOW/MID/FAST/REV) had no tooltip, unlike signal
  state buttons. Added i18n tooltips (`servo.*` keys, fr/de/es/en) in both the
  cockpit card and the board editor's bus device row. Validated on hardware.
  (#34)
- Devices with `default_state` ON can now stagger their activation instead of
  all popping on at once — new optional per-device `start_delay_ms` (fixed)
  and `start_delay_random_ms` (extra random, redrawn each time) config fields.
  The delay applies on every transition touching OFF, in either direction
  (OFF -> active, e.g. boot/hot-reload default state, ALL ON, DCC; and
  active -> OFF, e.g. ALL OFF, DCC) — but never on a change between two
  active states (e.g. SLOW -> FAST). A single manual click on a device's own
  icon in the cockpit always skips the delay (instant feedback), while ALL
  ON/OFF, group actions, and DCC continue to apply it. Each effect's
  coroutine applies it via a new `DEVICE_APPLY_START_DELAY()` macro placed
  right after its existing state-change wait — a no-op unless the transition
  qualifies, so no effect's own logic changed. New WebUI fields in the device
  editor (i18n ×4), plus a "délai" pin-label toggle button (alongside GPIO/DCC)
  on the Boards config tab showing each device's configured delay. Validated
  on hardware (6 staggered GasLamp on SPI, and again via ALL OFF/ALL ON).
  (#8)
- Board editor: the Buses tab's applicative bus list and its wizard "+ Add"
  linked-bus suggestions are now sorted alphabetically by key instead of
  following JSON insertion order. (#118)

## 2026-07-12

- Servo motor mode: a DCC reverse command no longer fails to reverse the
  servo's rotation. `SerialServoMotorMode::setSpeed()` was shared between the
  DCC path (always sends an explicit signed value) and the WebUI path (sends
  a magnitude only, relies on the current direction persisting) — split into
  `setSpeed()` (DCC, honours the sign as-is) and `setMotorSpeed()` (WebUI,
  keeps the current sign, updates magnitude only). `SERVO_PRESERVE_DIRECTION`
  is now unused by either path and has been removed entirely (code + docs).
  Validated on hardware via JMRI (DCC forward/reverse) and the WebUI. (#74)
- About page: the "Build" timestamp could stay frozen across builds that
  didn't happen to touch `DeviceStatusApi.cpp` itself (it read `__DATE__
  __TIME__`, stamped only when that translation unit gets recompiled). Now
  generated fresh on every build by `tools/gen_build_info.py`. (#108)
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
