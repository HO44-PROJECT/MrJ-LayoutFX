# Software architecture

Developer-facing notes on how the library is built (distinct from the user docs in `doc/`).

- [overview.md](overview.md) — the big picture: config-driven model (config.json → factory →
  coroutine devices), module layout, pins & buses, the boot sequence, persistence, and the
  embedded WebUI.
- [concurrency.md](concurrency.md) — the control model (UI / API / DCC → `newState` only, never
  raw pin access) and how bi-core concurrency is handled (the single device-list mutex, the benign
  `desiredState` field access).
- [memory.md](memory.md) — memory frugality by design: the smallest-type-that-fits rule (why
  `STATE_TYPE` stays `int8_t`), coroutines vs tasks, static allocation, compile-time stripping, and
  constants kept in flash (PROGMEM) rather than RAM.
- [audio-effects-design.md](audio-effects-design.md) — forward-looking design doc (not yet fully
  implemented) for the DFR1173 audio-effects feature: the `DfAudio`→`DfRobotSerialMP3` rename
  rationale, the `DfR1173Bus`/`DfRobotSerialMP3` protocol split, the fade-effect coroutine engine
  design (borrowed from `I2cPwmMotorDevice`), the new DCC loco-function dispatch hook, and the
  WebUI provisioning gaps.

_Add one focused `.md` per architectural topic here._
