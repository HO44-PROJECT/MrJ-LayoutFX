# Memory frugality by design

## Why it is a hard constraint
The same library targets an **ESP32 and an AVR Arduino Nano** (~2 KB RAM, 32 KB flash). The Nano
makes frugality non-negotiable; the ESP32 build inherits the discipline. Every choice favours the
**smallest footprint that fits — with a margin, not a hope**.

## Device state: the smallest type that fits
- `STATE_TYPE` is **`int8_t`** (one byte). A device has a handful of states (typically 0–4), and the
  state-machine sentinels (`OFF_STATE=0`, `RUN_STABLE_STATE=101`, `RUN_TRANSIT_STATE=-100`,
  `INIT_STATE=-101`) all sit well inside ±127. A device will never have hundreds of states, so the
  type is **deliberately not widened**.
- **Invariant:** any new sentinel (e.g. a future TEST / identify state) must fit `int8_t` — use the
  free headroom (`102..127`, `-128..-102`), never collide with the existing sentinels, and never
  reach for a wider type. Widening a per-device field across up to 256 devices for a value that fits
  in a byte is exactly the anti-pattern this principle rejects.
- Devices carry minimal state: the desired state, their pins, a few effect parameters.

## Coroutines, not tasks
Effects are **AceRoutine coroutines** driven by a single cooperative scheduler — not FreeRTOS tasks.
A task costs a dedicated stack + TCB (kilobytes each); a coroutine is a small object with a resume
point. Dozens of effects cost dozens of small objects, not dozens of stacks.

## Static allocation
The factory holds **fixed-size arrays** (`_devices[LFX_FACTORY_MAX_DEVICES]`, boards, buses…) sized
by compile-time limits. No heap churn, no fragmentation, a footprint you can reason about at build
time. Devices are created into these slots, not via scattered `new` on hot paths.

## Compile-time stripping
Feature `#define`s (`config.h` → `LFX_*_ENABLED` guards) compile **out** everything unused: no OLED
code without `OLED`, no HTTP/WiFi on the Nano, no DCC without `DCC_PIN`, and so on. A target only pays
flash/RAM for what it actually runs.

## Constants in flash, not RAM
- String literals go through **`F()` / `__FlashStringHelper`** → kept in flash, not copied to RAM.
- The catalog (`device_types.json`, `board_types.json`, `bus_types.json`, `i2c_known.json`) and the
  whole WebUI are **gzipped in PROGMEM** and served straight from flash — never inflated into RAM.
  (This is also why a catalog / WebUI change needs a reflash.)

## Bounded I/O
- The config is written in **fixed-size chunks** (`kFsChunkSize`), never by materialising the whole
  file in one buffer.
- The DCC audit dedup uses a **bitmap** (one bit per address), not a list of seen addresses.

## The rule
Prefer the smallest type / allocation that fits the real range, with a small margin; keep magic
values inside that range; static over dynamic; flash over RAM for anything constant; let `#define`s
remove what a target does not use. **When in doubt, size for the Nano.**
