# Adding a new device type — playbook

A worked checklist for adding a brand-new device/effect type to the firmware
and WebUI, using **issue #111 — `GasLampDefect`** as the running example.
Read [conventions.md](conventions.md) and
[../architecture/concurrency.md](../architecture/concurrency.md) first — this
document is the concrete step-by-step application of the rules they set out,
not a replacement for them.

## 0. Scope — what #111 actually asks for

> "Mix entre gas lamp et defect lamp" — same base behaviour as `GasLamp`
> (ignition → flicker → brightening → stable flame → extinction), but the
> stable flame occasionally suffers a rare, subtle malfunction, in the spirit
> of `DefectLamp`. **`DefectLamp` itself is not modified.** This is a brand
> new, fully independent device type, known to the UI exactly like every
> other effect — not a flag on an existing one.

Confirmed with the user: `DefectLamp` has no bug — its rapid, constant
flicker/outage cycling (10% stable→flicker, 80% flicker→off, 8% flicker→
stable, every 50 ms tick, no minimum dwell time — see
`include/led_fx/DefectLamp.h` + `src/led_fx/DefectLamp.cpp`) is its intended
character. The new effect must **not** reuse those constants or that
cadence; its malfunction must be rare (on the order of minutes between
glitches, not seconds) and short, layered on top of an otherwise normal
gaslight.

## 1. Design — coroutine structure

`GasLamp::runCoroutine()` (`src/led_fx/GasLamp.cpp`) is a single
`COROUTINE_LOOP()` with a `switch(getTargetState())` → `switch(getState())`
nest: `IGNITION` → `INITIAL_FLICKER` → `BRIGHTENING` → `STABLE_FLAME`, plus an
`OFF_STATE` branch running `extinctionStep()`. There is exactly one
`analogWrite`/`simulatePWM` call and one `COROUTINE_DELAY(delayMs)` call per
loop iteration, at the very end, shared by every phase.

The new class, `GasLampDefect`, is **not** a subclass of `GasLamp`: the
phases are not virtual/overridable hooks, they're inlined in one function, so
subclassing would only let us override the whole coroutine, buying nothing
over a fresh implementation. Instead:

- New pair `include/led_fx/GasLampDefect.h` / `src/led_fx/GasLampDefect.cpp`,
  modeled on `GasLamp.h`/`.cpp` line-for-line for the four existing phases
  and the extinction sequence (copy, don't inherit).
- Add one new state, `MALFUNCTION = NEXT_NON_STABLE - 3` (next free slot
  after `GasLamp`'s `BRIGHTENING = NEXT_NON_STABLE - 2`), entered only from
  `STABLE_FLAME` and always returning to `STABLE_FLAME`.
- In `STABLE_FLAME`, alongside the existing subtle-flicker roll, add a
  low-probability roll (see §2 for constants) that switches to
  `MALFUNCTION` instead of the normal subtle variation for that tick.
- `MALFUNCTION` drives brightness to near-zero (or a single out-of-band dip)
  for a short random duration, then sets state back to `STABLE_FLAME` and
  resets `startTime`/`flickerInterval` so the normal stable-flame cadence
  resumes cleanly — exactly the same re-entry pattern `GasLamp` already uses
  when moving between its own phases.
- `statusChar()` gets one more `case MALFUNCTION: return 'x';` (or similar),
  following `GasLamp.h`'s existing `i`/`f`/`b`/`F` convention.

**Coroutine rules this must respect** (from `conventions.md`):
- Never block: no `delay()`, no busy-wait. Everything goes through the
  existing single `COROUTINE_DELAY(delayMs)` at the loop's end, exactly like
  `GasLamp` already does — the new phase does not add a second delay call.
- Any value that must survive a yield (malfunction start timestamp,
  malfunction duration, saved pre-malfunction brightness if needed) is a
  **member**, declared alongside `GasLamp`'s existing `startTime` /
  `brightness` / `delayMs` / `flickerInterval` in the header — never a local
  inside the coroutine body.
- Pins are only ever touched via the one shared `analogWrite`/`simulatePWM`
  call already at the bottom of the loop — the new phase must not add a
  second, competing pin write.

## 2. New constants

Do not touch any existing `GASLAMP_*` constant (used by `GasLamp`) or any
`DEFECT_LAMP_*` constant (used by `DefectLamp`, and explicitly off-limits per
#111). Add a new, distinctly-named block to
`include/LayoutFX_default.h`, next to the existing `GASLAMP_*` block:

```cpp
#define GASLAMPDEFECT_MALFUNCTION_CHANCE 300        ///< Chance per stable-flame tick of a glitch, out of GASLAMPDEFECT_MALFUNCTION_CHANCE_RANGE
#define GASLAMPDEFECT_MALFUNCTION_CHANCE_RANGE 1000
#define GASLAMPDEFECT_MALFUNCTION_MIN_MS 80       ///< Malfunction duration range (ms) — short, not DefectLamp's 50-300ms outage
#define GASLAMPDEFECT_MALFUNCTION_MAX_MS 250
#define GASLAMPDEFECT_MALFUNCTION_MIN_BRIGHTNESS 0
#define GASLAMPDEFECT_MALFUNCTION_MAX_BRIGHTNESS 30
```

(Exact names/values to be tuned once on hardware — the point is a dedicated
namespace so nothing here can silently affect `GasLamp` or `DefectLamp`.)

**Pitfall hit during implementation:** the roll is per stable-flame *tick*
(every `flickerInterval`, ~500-1000ms, ~750ms average — not per millisecond),
so a "chance per tick" constant must be converted to a real-world rate before
picking a number, or it silently lands orders of magnitude off. The first
value shipped (`1/1000` ≈ 0.1% per tick) computed out to roughly one glitch
every 8-16 *minutes* — invisible in a normal hands-on test, which the user
correctly read as "nothing is happening." Corrected to `17/1000` ≈ 1.7% per tick ≈ one glitch per 30-60s — still too
subtle for the user's taste on real hardware, raised to `100/1000` = 10% per
tick ≈ one glitch per 5-10s, then raised again to `300/1000` = 30% per tick
≈ one glitch per 2-3s, the value actually kept. At this rate the effect
reads as a constant, prominent malfunction rather than an occasional one —
worth confirming on hardware whether this still feels like "gas lamp with a
rare defect" or has crossed into "defect lamp with a gas-lamp base," which
would start to blur the distinction the user drew with #111's actual
`DefectLamp`. **Rule going forward: before shipping any per-tick probability,
compute mean-time-between-events = tick-period / probability, and
sanity-check that number in plain units (seconds/minutes) against how the
effect will actually be tested.**

## 3. Device registration — the six/seven places

Per `conventions.md`'s "device type string appears in six places" rule, plus
the factory dispatch include, applied to `GasLampDefect`:

| # | File | Change |
|---|------|--------|
| 1 | `data/device_types.json` | new entry, copy `GasLamp`'s shape: `"GasLampDefect": { "label": "Gas Lamp (defective)", "category": "light", "wires": 1, "ctor_style": "single_pin" }` |
| 2 | `include/config/DeviceFactoryKeys.h` | `constexpr char kDevGasLampDefect[] = "GasLampDefect";` next to `kDevGasLamp` |
| 3 | `src/config/DeviceFactory.cpp` | `#include "led_fx/GasLampDefect.h"` + `else if (strcmp(type, kDevGasLampDefect) == 0) d = new GasLampDefect(_pin(wiring, boardIdx));` next to the `kDevGasLamp` branch; also add to the single-pin device list comment (~line 666) |
| 4 | `include/LayoutFX.h` | `#include "led_fx/GasLampDefect.h"` next to the `GasLamp`/`DefectLamp` includes |
| 5 | `src/oled/OledDisplay.cpp` | new icon-drawing branch (`strcmp(type, factory_keys::kDevGasLampDefect) == 0`) — reuse the gas-lamp glyph plus `DefectLamp`'s X-cross overlay (both already drawn in this file, ~line 676 and ~line 705) so the OLED glyph visually communicates "gas lamp, but defective" |
| 6 | `src/web/icons.js` | new `'GasLampDefect': …` SVG entry, reusing/adapting the existing `'GasLamp'` path (~line 35) the same way OLED reuses its glyph |
| 7 | `schemas/config.schema.json` | add `"GasLampDefect"` to the `devices[].type` `enum` list (alongside `"GasLamp"`, `"DefectLamp"`, etc.) — this field **is** enum-gated, unlike the top-level `$schema`/`name` strings |

Also add the fr/de/es/en labels to `src/web/i18n.js`, next to the existing
`'GasLamp'` entries (lines 21/46/71/96) — e.g. `'GasLampDefect': 'Lampe à gaz
défectueuse'` (fr) and equivalents for de/es/en. This isn't one of the
"six places" in `conventions.md` because that list already implies i18n is
covered by "the fr/de/es/en labels" item — restated here so it isn't missed.

## 4. i18n

Per `conventions.md`'s "i18n by design" rule: the label added in step 3 must
land in all four languages **in the same change**, not as a follow-up. No
other new user-visible string is introduced by this effect (no new tooltip,
no new API field), so this is the only i18n surface.

## 5. UI parity

The goal stated by the user is that this behaves as a first-class effect,
"connu de l'UI avec le même design que les autres effets" — meaning no
special-casing anywhere in the WebUI beyond the standard per-type
icon/label/tooltip lookups already keyed by `type` string. Once steps 3–4 are
done, the device editor, the dashboard tile, and the OLED status screen pick
it up automatically the same way `GasLamp`/`DefectLamp` already do — there is
no separate "UI change" beyond the catalog/icon/i18n entries above. Confirm
this by comparing the rendered device editor entry for `GasLampDefect`
against `GasLamp`'s once the WebUI is rebuilt.

## 6. Validation (no `pio run`)

Per the standing process, builds/flashes are done by the maintainer only.
Cheap local checks before handing off:
- `python3 -c "import json; json.load(open('lib/MrJ-RailwayFX.local/data/device_types.json'))"`
- `python3 -c "import json; json.load(open('lib/MrJ-RailwayFX.local/schemas/config.schema.json'))"` (should be unaffected, but confirm it still parses)
- `node --check lib/MrJ-RailwayFX.local/src/web/icons.js`
- `node --check lib/MrJ-RailwayFX.local/src/web/i18n.js`
- Manual brace/paren balance review of `GasLampDefect.cpp`/`.h`,
  `DeviceFactory.cpp`, `OledDisplay.cpp` (no local compiler for the AVR/ESP32
  targets).

## 7. Process (per project standing rules)

- Issue #111 already exists on the backlog board — move it Todo → In
  Progress when work starts.
- One commit per repo touched (main project vs. `lib/MrJ-RailwayFX.local`) —
  this change is entirely inside the library, so a single commit there
  covers it, unless a `configurations/<env>/` example/demo is added too.
- Do not close the issue / update `CHANGELOG.md` / commit until the user has
  built, flashed, and validated the new effect on real hardware.
- No `Co-Authored-By` / "Generated with Claude" lines in the commit.
