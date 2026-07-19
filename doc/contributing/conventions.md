# Project conventions

[Docs](../README.md) / [Contributing](README.md) / Conventions

The rules below aren't enforced by a compiler — they're the shared habits that
keep a config-driven, multi-target, multi-language firmware coherent. Most bugs
that survive a build come from breaking one of these.

## Two git repositories

The tree is two independent repositories:

- **The main project** (branch `main`) — `platformio.ini`, the per-environment
  `configurations/<env>/`, the main-project `scripts/`, and the user-facing
  wiring. It **ignores `lib/`** (and `.pio/`), so the library is not a submodule
  or a subtree — it's a sibling repo checked out inside `lib/`.
- **The library** `lib/MrJ-RailwayFX.local/` (branch `v1`) — all the reusable
  firmware: `src/`, `include/`, `data/`, `src/web/`, `tools/`, and this `doc/`.
  Almost all day-to-day code and docs live here.

A change that spans both (e.g. a new env plus its device support) is two commits,
one per repo. PlatformIO discovers the library automatically because it sits
under the project's `lib/` — no `lib_extra_dirs` needed.

## Source tree layout (in the library)

| Directory | Holds |
| --- | --- |
| `src/` | `.cpp` implementation, grouped by area (`api/`, `config/`, `oled/`, `utils/`, `web/`, …) |
| `include/` | public headers, mirroring the `src/` layout; consumers include by area, e.g. `"config/ConfigManager.h"` |
| `include/generated/` | **machine-generated** headers only — gitignored, never edited (see [build-pipeline.md](build-pipeline.md)) |
| `data/` | the JSON catalogs that are the *source of truth* for device/board/bus types |
| `src/web/` | the WebUI sources (HTML/CSS/JS) that get bundled into the firmware |
| `schemas/` | `config.schema.json` — the JSON schema the configs validate against |
| `tools/` | the pre-build code generators |
| `doc/` | user docs (`user/`, `advanced/`) plus these developer docs under `contributing/` (`architecture/` = how it runs, the rest = how it's built) |

The per-environment `main.cpp` and `config.h` live in the **main project** under
`configurations/<env>/`, not in the library (see [platformio.md](platformio.md)).

## Generated files are outputs, not sources

Anything under `include/generated/` is produced by a pre-build hook from a real
source (`data/*.json`, `src/web/*`, `platformio.ini`). It is gitignored,
regenerated on every build, and must never be hand-edited or committed. If a
value looks wrong there, fix the *input* and rebuild. Headers are consumed via
the `"generated/…"` include path so the generated origin is obvious at the call
site.

## Control reaches devices only through `newState`

Every actuation path — the WebUI, the REST API, DCC, the boot default states —
must drive a device through its `newState()` / `switchOn()` primitives, **never**
by touching pins or effects directly. This is a hard design rule, not a style
preference: it is what makes a single mutex sufficient for cross-core safety. The
full rationale and the concurrency model are in
[`architecture/concurrency.md`](architecture/concurrency.md).

## Writing device coroutines

Device effects are AceRoutine **stackless coroutines**: cooperatively scheduled one
step at a time, all on the same core, sharing it with every other effect. Two rules
are non-negotiable — each was learned the hard way, because breaking either makes a
*single* effect corrupt *every* other effect.

**Never block. Never `delay()` / `delayMicroseconds()`.** A coroutine must *yield*
time back to the scheduler, never *consume* it. Use the non-blocking primitives only —
`COROUTINE_DELAY`, `COROUTINE_DELAY_MICROS`, `COROUTINE_AWAIT`, and the `simulatePWM*`
macros (which wrap `COROUTINE_DELAY_MICROS`). A raw `delay()` / `delayMicroseconds()` —
or any busy-wait, or a blocking bus read/write — freezes the **whole core** for its
full duration: every other coroutine's software PWM stops dead, and because those PWMs
are timing-critical the result is visible flicker/stutter across *all* effects at once,
not just the offending one. A single blocking effect is enough to wreck every fade on
the layout. Keep blocking peripheral I/O (a serial read with a timeout, a full I²C
display refresh) off the coroutine path, or make it non-blocking.

**State that must survive a yield lives in a member, not a local.** Stackless
coroutines do not preserve the C++ stack across a yield: any local variable declared
before a `COROUTINE_*` yield point holds garbage when the coroutine resumes. Every
value that must persist across a yield — phase, brightness, timers, counters — must be
a **private member** of the device class. A local is safe only for a value computed and
fully consumed *between* two yields (never across one).

See [`architecture/concurrency.md`](architecture/concurrency.md) for the
single-core cooperative-scheduler model these two rules follow from.

## Memory frugality by design

Types, allocation, and I/O are sized for the smallest target (the AVR Nano):
smallest-integer-that-fits, static allocation over the heap, constants in flash
(`F()` / `PROGMEM`), compile-time stripping of disabled features, and bounded,
chunked filesystem writes. When in doubt, size for the Nano. The reasoning is in
[`architecture/memory.md`](architecture/memory.md).

## i18n by design

There is no hard-coded UI text. Every user-visible string is a translation key
present in all four languages (fr / de / es / en) in `src/web/i18n.js` from the
moment it's introduced — never an inline label added "to translate later".

## Consistency rules that span several files

Some identifiers are duplicated across layers by necessity; changing one without
the others yields a half-working device or a silent mismatch.

**A device `type` string appears in six places** — all must agree exactly:

1. `data/device_types.json` — the catalog entry (drives the WebUI editor)
2. `include/config/DeviceFactoryKeys.h` — the factory dispatch key
3. `src/oled/OledDisplay.cpp` — the OLED state name and icon selection
4. `src/web/icons.js` — the icon map
5. `src/web/i18n.js` — the fr/de/es/en labels
6. `schemas/config.schema.json` — schema validation

Add or rename a device type → update all six.

**Feature badges mirror compile-time flags.** The badges the UI shows are a
compile-time reflection of the `#define`s set in a configuration's `config.h`.
The authoritative inventory of those flags is
`include/LayoutFX_define.h`; adding a flag/badge means updating the coordinated
set of files that surface it, keeping the define, its badge, and its UI text in
step. See [`../advanced/configuration-flags.md`](../advanced/configuration-flags.md) for the flag
catalog.

## Build / validate / commit workflow

Builds are run by the maintainer, not automatically as part of a change: edits
are prepared, the maintainer builds and validates on real hardware, and **only
then** is the change committed. Validate non-buildable edits cheaply where
possible — `node --check` for JS, a JSON parse for catalogs/schemas,
`python3 -m py_compile` for the hooks — so a build isn't spent finding a typo.
