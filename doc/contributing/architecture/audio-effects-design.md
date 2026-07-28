# Audio effects — design & strategy (DFR1173)

[Docs](../../README.md) / [Contributing](../README.md) / [Architecture](README.md) / Audio effects design

> Forward-looking design doc, not a record of what's shipped yet. Written to
> capture the analysis and decisions made while scoping the audio-effects
> feature, before the remaining implementation work (fade engine, DCC function
> mapping, WebUI provisioning) is built. Tracked by backlog
> [#9](https://github.com/HO44-PROJECT/MrJ-LayoutFX-backlog/issues/9) (rework)
> and [#103](https://github.com/HO44-PROJECT/MrJ-LayoutFX-backlog/issues/103)
> (ideation).

## 1. Where this started

The audio device driver predates this design pass and was functionally
minimal: turn a track on/off from a DCC accessory address, nothing else
wired up. Two problems made it hard to build on:

- **Naming didn't match reality.** The device class was called `DfAudio` and
  the board-type catalog entry `"DfPlayerMini"`, but the actual hardware is a
  **DFRobot DFR1173** ("MP3 Voice Prompter"), which is a *different* serial
  protocol from the classic DFPlayer Mini/YX5200 module (7-byte frames with
  no checksum, vs. the DFPlayer's 10-byte frames with a 16-bit checksum).
  Anyone porting the official DFPlayer driver against this class would get
  it wrong.
- **Protocol and device concerns were fused.** The old `DfAudio` class held
  both the raw serial command set *and* the `MultiplePinDevice`/DCC
  integration in one file, making it awkward to reason about or extend.

### 1.1 Rename: `DfAudio` → `DfRobotSerialMP3`

Renamed end-to-end, no backward-compatibility shim (config-driven factory
pattern means the class name **is** a literal string key matched via
`strcmp` in `DeviceFactory.cpp`, so schema/data must match exactly):

| Old | New |
|---|---|
| `DfAudio` (class, files, `kDevDfAudio` factory key) | `DfRobotSerialMP3` |
| `LFX_AUDIO_ENABLED` (feature macro) | `LFX_SERIAL_AUDIO_ENABLED` |
| `"DfPlayerMini"` (board-type catalog key) | `"DfR1173"` |

Touched: `include/audio/DfRobotSerialMP3.h`, `src/audio/DfRobotSerialMP3.cpp`,
`include/LayoutFX_define.h`, `include/LayoutFX.h`,
`src/config/DeviceFactory.cpp`, `include/config/DeviceFactory.h`,
`include/config/DeviceFactoryKeys.h`, `src/api/DeviceStatusApi.cpp`,
`schemas/config.schema.json`, `schemas/device_types.schema.json`,
`data/device_types.json`, `data/board_types.json`, `src/web/icons.js`,
`src/web/app-boards.js`, `src/web/i18n.js` (including the `abt.feat.audio`
About-page strings across all 4 locales — easy to miss because that key's
*name* doesn't contain "DfAudio"/"DfPlayerMini", only its *value* did),
`doc/user/device-types.md`.

The board-type rename was safe as pure data/UI because `DeviceFactory.cpp`
never matches boards by their `type` string literal — only by `busType`
(an enum). Only the device-type rename touched the factory's `strcmp` path.

### 1.2 Protocol/device split (done just before this design pass)

`DfAudio` was split into:

- **`DfR1173Bus`** (`include/audio/DfR1173Bus.h` / `src/audio/DfR1173Bus.cpp`)
  — pure protocol layer. Owns the `SoftwareSerial` link, implements the
  DFR1173's frame protocol: `playTrack`, `nextTrack`, `previousTrack`,
  `pausePlayback`, `resumePlayback`, `stopPlayback`, `setVolume`,
  `increaseVolume`, `decreaseVolume`, `repeatPlayback`, `randomPlayback`,
  `playSpecificFolder`, `compositePlayback`, `resetModule`,
  `enterLowPowerMode`, `sendCommand`, `waitForAck`. No device/DCC coupling.
- **`DfRobotSerialMP3`** (`include/audio/DfRobotSerialMP3.h` /
  `src/audio/DfRobotSerialMP3.cpp`) — the `MultiplePinDevice` wrapper. Holds
  a `DfR1173Bus *bus` and delegates every playback call to it; owns device
  naming and DCC accessory/speed integration.

This split is what makes the rest of this design tractable: the fade-effect
engine and DCC dispatch below are pure additions to `DfRobotSerialMP3`
(the device layer), with zero changes needed to `DfR1173Bus` (the protocol
layer).

## 2. Feature scope: what "rich audio effects" means here

Starting point (user-specified scenarios, in priority order):

1. **DCC → play track**, with an optional repeat and an optional duration.
2. **DCC → play folder**, with optional repeat, random, and duration.
3. **DCC → next track.**
4. **DCC → previous track.**
5. **Volume variation as a first-class effect** — concretely, a level-crossing
   /train-passing effect: play a sound with **fade in, hold, fade out**,
   rather than a flat on/off.

Plus two DCC *response paths*, distinct from each other and from what
exists today:

- **Accessory (on/off)** — already wired (`setDccAccessoryState`), but
  currently hardcoded to one track/volume. Needs to become configurable:
  which track/folder to play, and whether "on" plays vs. stops.
- **Loco function mapping** — map individual loco functions (F0-F28) to
  audio actions: next, prev, random, repeat. This does not exist yet at all
  (see §4).

A secondary, not-yet-confirmed path: **volume via loco speed** — already
partially wired (`setDccSpeed` calls `setVolume(Speed)` directly, marked
`// TODO: map?`) but not actually mapped/clamped into a sensible volume
range, and conceptually distinct from the fade effect (this is a continuous
"throttle controls loudness" behavior, e.g. for a Doppler-style
train-passing simulation driven by the loco's actual speed rather than a
scripted fade curve).

Other ideas raised but **not yet confirmed** with the user — flagged here so
they aren't lost, not decided:
- Loop-while-accessory-active with a clean stop-fade (vs. today's abrupt
  `stopPlayback()`).
- Randomized trigger delay (avoid multiple instances of the same effect
  firing in lockstep across a layout).
- Cooldown / anti-retrigger debounce (ignore a re-trigger while a track is
  already mid-play).

## 3. Fade-effect engine — design

### 3.1 Why `I2cPwmMotorDevice` is the template

`DfRobotSerialMP3::runCoroutine()` is currently a no-op stub (`return 0;`).
`include/servo/I2cPwmMotorDevice.h` / `.cpp` already solve almost exactly
this problem for PWM motor speed: a ramp state machine with hold and
auto-ramp-down, running on an `ace_routine::Coroutine`. Substituting
"volume (0-30)" for "PWM microseconds" carries the design over directly:

- **State table**: a small fixed array of named presets (`MotorState`-style
  struct), each with `speed`/`volume` target, `duration_ms` (hold length,
  `0` = perpetual/no auto-off), `ramp_up_ms`, `ramp_down_ms`, and a
  `label[16]` for the WebUI. `MAX_STATES = 8` in the motor case; audio
  likely needs fewer slots since a per-device effect list, not a
  general-purpose preset bank, is the target.
- **Coroutine tick**: `COROUTINE_LOOP()` + `COROUTINE_DELAY(20)` (20ms),
  linear interpolation of the current volume toward the target every tick.
- **Members, not locals**: every piece of state that must survive a
  `COROUTINE_DELAY` suspension (`_currentVolume`, `_targetVolume`,
  `_rampFromVolume`, `_rampStartMs`, `_rampDurMs`, `_runDurMs`,
  `_runStartMs`, `_stopRampDownMs`) must be a class member — AceRoutine
  coroutines do not preserve local stack variables across a suspension
  point. This is the single most important constraint carried over from the
  motor implementation, and it's explicitly documented there for the same
  reason.
- **Interruption behavior**: retriggering mid-ramp starts a **new** ramp
  from the *current* in-between value toward the new target — no snapping.
  This is exactly what "a second train passes while the first fade-out is
  still running" needs.
- **1-based state selection**: `newState(s)` indexes `_states[s-1]`, matching
  the motor device's convention (kept for consistency with the rest of the
  codebase, not an inherent requirement of the pattern).

### 3.2 What's genuinely different from the motor case

- The target quantity is **volume** (`DFAUDIO_VOLUME`, `uint8_t`, 0-30 per
  the DFR1173 protocol) instead of a PWM pulse width — same linear-ramp math,
  smaller/different range.
- A fade effect is layered *on top of* playback control, not a replacement
  for it: starting a fade-in effect needs to also call `bus->playTrack(...)`
  (or `playSpecificFolder`) at ramp start, and the ramp-down's completion
  needs to trigger `stopPlayback()` — the motor device has no analogous
  "also do a side-effecting action at ramp start/end" requirement.
- `duration_ms == 0` (perpetual hold) doesn't obviously make sense for a
  one-shot sound effect the way it does for a motor left running — worth
  re-confirming against the "play track (option repeat) (option durée?)"
  scenario, where duration may instead mean "stop repeating after N ms"
  rather than "hold at volume forever."

### 3.3 Not yet implemented

No code has been written for this engine yet. This section is a design
carried over from the `I2cPwmMotorDevice` precedent, confirmed as the right
template, but `DfRobotSerialMP3::runCoroutine()` still needs the actual
state table, member fields, and tick logic.

## 4. DCC loco-function mapping — design

### 4.1 Why a new hook, not `setDccFunction`

`DccDrivable` already exposes three **separate** DCC dispatch paths
(`include/dcc/DccDrivable.h`, `src/dcc/DccDrivable.cpp`):

1. `notifyDccState(Addr, State)` → `setDccFunction(uint8_t State)` /
   `setDccAccessoryState(uint8_t State)` — accessory/turnout-address based
   on/off. `DfRobotSerialMP3` already uses `setDccAccessoryState`.
2. `notifyDccSpeed(Addr, AddrType, Speed, Dir, SpeedSteps)` →
   `setDccSpeed(int16_t mappedSpeed)` — loco-address based, maps 14/28/128
   speed-step packets into `[-1000, 1000]`. Already tapped by
   `DfRobotSerialMP3::setDccSpeed` (currently unmapped, see §2).
3. `notifyDccFunc(Addr, AddrType, FN_GROUP FuncGrp, uint8_t FuncState)` —
   loco-address based, F0-F28 function-group packets. **Currently dispatches
   to no device at all** — `DccDrivable.cpp` only logs the packet
   (`logDccEvent(DCC_MSG_FUNC, Addr, FuncState, nullptr)`), with an explicit
   comment noting "No registered device currently reacts to raw
   function-group packets (`setDccFunction` is wired from `notifyDccState`,
   a distinct legacy path)."

`setDccFunction(uint8_t State)` exists on `DccDrivable` (line 257) but is
wired from path (1), not path (3), and its single `uint8_t State` parameter
can't identify *which* function number changed — unusable for "map function
12 to next-track, function 13 to previous-track." Its only current
implementer is `LedEffect.h`, which this work leaves untouched.

**Decision: build a new, purpose-built hook** — tentatively
`setDccFunctionState(uint8_t funcIndex, bool on)` — wired from
`notifyDccFunc()`, decoding `FuncGrp`/`FuncState` into a single 0-28 function
index before calling it. This leaves the existing `setDccFunction` /
`notifyDccState` path completely untouched.

### 4.2 NMRA function-group encoding (groundwork, now complete)

From the vendored `NmraDcc` library
(`.pio/libdeps/*/NmraDcc/NmraDcc.h`, a PlatformIO dependency, not part of
this repo's own source):

```cpp
typedef enum {
    FN_0_4 = 1,
    FN_5_8,
    FN_9_12,
    FN_13_20,
    FN_21_28,
    #ifdef NMRA_DCC_ENABLE_14_SPEED_STEP_MODE
    FN_0,
    #endif
    FN_LAST
} FN_GROUP;

#define FN_BIT_00  0x10
#define FN_BIT_01  0x01
#define FN_BIT_02  0x02
#define FN_BIT_03  0x04
#define FN_BIT_04  0x08
// ... one FN_BIT_xx per function within each group, documented per-group in NmraDcc.h
```

`FuncState` is a **per-group bitmask byte**, not a single flag — each bit
corresponds to one function number within that packet's group. Decoding a
function index therefore needs both `FuncGrp` (which group/range) and the
specific bit within `FuncState` that changed, e.g.:

```cpp
// sketch, not yet implemented
void DccDrivable::notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType,
                                 FN_GROUP FuncGrp, uint8_t FuncState) {
  trackDccMsg(DCC_MSG_FUNC);
  // ... existing audit/logging unchanged ...
  uint8_t baseFn = functionGroupBase(FuncGrp); // FN_0_4->0, FN_5_8->5, FN_9_12->9, ...
  for (uint8_t bit = 0; bit < bitsInGroup(FuncGrp); bit++) {
    bool on = FuncState & (1 << bit);
    dispatchToDeviceAt(Addr, [&](DccDrivable *dev) {
      dev->setDccFunctionState(baseFn + bit, on);
    });
  }
}
```

The exact bit-to-function-number mapping per group (the `FN_BIT_*`
constants) still needs a careful read-through before writing this for real
— `FN_0_4`'s bits are documented as **not** in numeric order (`FN_BIT_00 =
0x10` sits in the middle of the byte, not bit 0) — this is the one detail
in the sketch above (`1 << bit`) that is almost certainly wrong and must be
replaced with an explicit per-group lookup table, not a shifted-bit
assumption.

### 4.3 Not yet implemented

No code has been written for `setDccFunctionState()`, its `DccDrivable.h`
declaration, or its `notifyDccFunc()` wiring. The per-group bit-mapping
table (§4.2) is the remaining blocker before this can be written correctly.

## 5. WebUI: provisioning flow

Goal: a "+Add" button on the DFR1173 board card, matching the existing
Lobot Servo Chain UX, opening the same generic device-editor modal used
for every device type.

### 5.1 What already works in the WebUI's favor

- The device-editor modal (`#de-modal` in `webui.html`, logic in
  `src/web/app-device-editor.js`) is already fully generic — one modal
  reused for add *and* edit of every device type, differentiated only by
  whether an existing `dev` object is passed to `openDevEditor(...)`.
  No new modal is needed.
- `_deAllowedTypes()` (`app-device-editor.js:34`) already allow-lists
  `category: 'audio'` for UART-bus boards — once the board's `+Add` button
  exists, the type picker will already correctly surface
  `DfRobotSerialMP3`.
- The WebUI branches on the device-type catalog's `category` field
  (`data/device_types.json`), never on `ctor_style` (confirmed: zero JS
  references to `ctor_style` anywhere — it's `config_to_cpp.py` code-gen
  metadata for a tool that doesn't exist yet). `DfRobotSerialMP3`'s
  `category` is already `"audio"`.

### 5.2 Concrete gaps (nothing here is implemented yet)

- **`data/board_types.json`: `DfR1173` still has `"no_devices": true`.**
  This is the single blocker suppressing the `+Add` button entirely —
  `renderDipPcb()` in `app-boards.js` gates the button on
  `(def.no_devices ? '' : ...)`. Needs to change (remove the flag, or add
  whatever the bus-board equivalent of "supports devices" is called for
  rows=0 boards like the Lobot chain).
- **No `AUDIO_TYPES` filter array** — the Lobot flow likely has a
  `SERVO_TYPES`-equivalent array driving which `+Add` call gets wired to the
  board; audio needs its own, or to reuse the generic allow-list path if one
  exists.
- **`renderExtraGrp()` (`app-device-editor.js`)** — no audio-specific extra
  fields yet (track/folder number, repeat/random/duration toggles, fade
  in/hold/out timings). Falls through to empty/generic behavior today.
- **`saveDevEditor()`** — no audio-specific serialization of those new
  fields into the device's config entry.
- **Cockpit `card()` dispatcher (`app-core.js`)** — no renderer for
  `category === 'audio'` yet; falls through to generic/empty.
- **`renderBusDevice()` (`app-boards.js`)** — the bus-board device-row
  renderer has an `isServo`-only branch for speed-preset buttons
  (~lines 353-362) and a generic DCC-address-badge fallback
  (~lines 344-349); audio devices currently only get the generic fallback.
  Needs an audio-specific row (e.g. play/pause/next/prev/volume controls).
- **`_dePinHint()`** — may need an audio-specific hint string, not yet
  checked in detail.

### 5.3 Sequencing

Per an explicit user decision, firmware/data-model work (fade engine, DCC
function mapping, richer `setDccAccessoryState`/track-folder config) is
being designed and built **before** the WebUI provisioning flow, so the UI
is built against a stable, already-decided device config shape rather than
guessing at fields that later change.

## 6. Status summary

| Area | Status |
|---|---|
| `DfAudio` → `DfRobotSerialMP3` rename (class, macro, schema, i18n, docs) | Done |
| `DfPlayerMini` → `DfR1173` board-type rename | Done |
| `DfR1173Bus` / `DfRobotSerialMP3` protocol/device split | Done |
| Fade-effect engine (`runCoroutine()` ramp state machine) | Designed, not implemented |
| DCC scenarios 1-4 (play track/folder, next, prev, with repeat/random/duration options) | Not implemented |
| `setDccFunctionState()` new hook + `notifyDccFunc()` wiring | Designed, blocked on per-group bit-mapping table |
| DCC speed → volume mapping (currently unmapped passthrough) | Not implemented |
| WebUI "+Add" provisioning flow for DFR1173 | Gaps enumerated (§5.2), not implemented |

## 7. Open questions (not yet decided)

- Does `duration_ms == 0` (perpetual) make sense for a one-shot sound
  effect, or should "duration" always mean "auto-stop/un-repeat after N ms"
  for audio specifically (§3.3)?
- Which of the additional ideas in §2 (loop-with-clean-stop-fade,
  randomized trigger delay, retrigger debounce) should actually be built,
  vs. left as future backlog items?
- Exact final signature of `setDccFunctionState()` — `(uint8_t funcIndex,
  bool on)` is the working assumption but not yet locked in against the
  per-group bit-mapping work in §4.2.
