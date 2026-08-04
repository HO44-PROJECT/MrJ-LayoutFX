# Troubleshooting — investigation logs

Post-mortems of real debugging sessions: what was observed, tested, ruled out and
concluded — kept so the same ground is never covered twice. This section completes
the developer docs: [`../architecture/`](../architecture/) describes how the
firmware *runs*, [the rest of `contributing/`](../) how it is *built*, and this
one what happened when it *misbehaved*.

Unlike the architecture docs, these are **historical records**, not structural
descriptions: chronology, dead ends, backlog card numbers and hardware photos all
belong here. An entry may be written in French or English — whatever served the
investigation.

## Investigations

- [spi-595-daisy-chain-investigation.md](spi-595-daisy-chain-investigation.md) —
  ✅ resolved — "outputs 9-16 of a 2×74HC595 chain can't be driven from the
  UI": a long software investigation (transfer methods, clock, refresh rate,
  subsystems disabled one by one)… concluded as a **hardware** fault on the
  breadboard, masked by a stale baseline ("the POC works here") that was never
  re-checked. Contains the methodology lesson and the repair checklist.
- [dcc-log-ring-buffer-eviction.md](dcc-log-ring-buffer-eviction.md) —
  ✅ resolved — "the DCC diagnostics log shows no trace during rapid
  signal/accessory toggles": the log's ring buffer was a single pool shared
  across the 5 message categories; high-throughput Speed/Func traffic could
  evict a rare Signal/Accessory event before the WebUI's polling had picked it
  up. Fixed with a per-category buffer (#76).
- [pca9685-oscillator-calibration.md](pca9685-oscillator-calibration.md) —
  ✅ resolved — "all servos/continuous motors on one PCA9685 board share the
  same shifted neutral point": the chip's internal oscillator (nominal 25 MHz,
  ±10% tolerance) runs at a different frequency than the firmware assumes.
  Measure the real neutral + compute `oscillator_hz`, configurable per board
  in `config.json`.
- [beacon-turnsignal-flicker-analysis.md](beacon-turnsignal-flicker-analysis.md) —
  🔍 in progress — "TurnSignal flickers when Beacon is active" on a 74HC595
  SPI chain: full diagnosis of the AceRoutine scheduler and software PWM over
  SPI, software fix validated (`COROUTINE_DELAY_MILLIS` → `COROUTINE_DELAY`).
  A purely hardware residual remains (breadboard coupling suspected between
  wiring 8 and wiring 16) — tracked by
  [#139](https://github.com/HO44-PROJECT/MrJ-LayoutFX-backlog/issues/139).

## Writing a new entry

One `.md` per investigation, named after the *symptom*. Recommended shape (see the
SPI entry): a status banner at the top (✅ resolved / 🔍 in progress) with the
final conclusion first, a TL;DR, the setup, a **table of every test with its
result**, what was formally ruled out, the retained explanation, and the
lessons learned. Write it as it happens — the dead ends are the valuable part.
