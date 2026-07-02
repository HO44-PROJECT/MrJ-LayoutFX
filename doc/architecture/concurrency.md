# Concurrency & control model

## The one rule
All control — WebUI, HTTP API, DCC, boot defaults — reaches a device **only** through its
state primitives: `newState(STATE_TYPE)` (and `switchOn()` / `switchOff()`, which call
`newState`). **Nothing outside a `Device` ever writes its pins or effect internals directly.**

Consequence: the only mutable device state touched from outside is `desiredState`, set through
`newState`. So **`newState` is the single concurrency point** — there is no hidden pin access to
race against. A test / diagnostic behaviour is therefore a coroutine device like any other, not a
side loop that drives pins itself.

## Control kinematics — who calls `newState`
- **WebUI → HTTP API** (Core 0): `/api/switch`, `/api/device`, `/api/all`, `/api/group`,
  `/api/servo` handlers → `device->newState(state)`.
- **DCC** (Core 1): NmraDcc ISR buffers bits → `dcc.process()` in `MrJFX::loop()` → `notifyDcc*`
  → `DccDrivable` dispatch by accessory address → the device's `setDccSigOutputState` /
  `setDccSpeed` / `setDccFunction`, which set state via `newState`.
- **Boot / hot-reload** (Core 1): config → `DeviceFactory` builds devices → `applyDefaultStates()`
  → `newState(default_state)`.
- **The effect itself** (Core 1): each device's `runCoroutine()` reads `desiredState`, walks its
  own state machine, and is the **only** writer of its pins (a GPIO, or the SPI 74HC595 buffer
  flushed once per loop).

## State machine — `newState` → convergence
`newState(s)` only records the **intent** (`desiredState = s`); the coroutine converges toward it:
`INIT → transit (busy) → stable`. Sentinels (`devices/Device.h`): `OFF_STATE = 0`,
`RUN_STABLE_STATE = 101`, `RUN_TRANSIT_STATE = -100`, `INIT_STATE = -101`; a **negative** state
means "in transition" (`busy()`). Control is therefore fire-and-forget: set the target, the
coroutine catches up over the next loops; repeated `newState` calls just move the target
(it converges to the last request — which is why buttons stay responsive during a transition).

## Bi-core layout
- **Core 0** (PRO_CPU, the WiFi core): the WiFi stack + a "system" FreeRTOS task running
  DNS (`processNextRequest`) + `WebServer::handleClient()` (the API handlers). Effects are never
  starved by the network.
- **Core 1** (APP_CPU, Arduino `loop()`): `CoroutineScheduler::loop()` (every effect),
  `DccDrivable::loop()` (DCC), `ConfigManager::handlePendingReload()` (hot-reload), OTA.
- So **effects + DCC + hot-reload all run on Core 1, sequentially** (one thread) → no races
  among them.

## The only cross-core interactions
1. **Device-list lifetime.** Core 0 handlers iterate `_factory` and dereference `Device*`, while
   the Core 1 hot-reload (`fullReset`) **deletes every `Device`**. Concurrent → use-after-free.
   - **Guard:** a single mutex `g_mrjfxDeviceMutex`, taken via `MRJFX_DEVICE_LOCK()` around
     `handleClient()` (Core 0) and around `fullReset` + rebuild (Core 1) → mutually exclusive.
     ESP32-only; a no-op on AVR (single-core, no HTTP). No deadlock: the reload is deferred through
     the `_reloadPending` flag, never called from within a handler.
   - **The effect coroutines NEVER take this lock.** By design: an effect must never be blocked by
     the network or a reload (that would starve every effect → glitches). It is safe lock-free
     because the reload runs **sequentially with the scheduler on the same Core 1 thread** — when
     `fullReset` deletes devices, no coroutine is executing. The mutex is purely the cross-core
     guard (Core 0 `handleClient` ↔ Core 1 `fullReset`). **Invariant:** keep the reload and the
     coroutines on the same Core 1 thread; moving the reload to another task/core would break this
     and force the coroutines under the lock.
2. **The `desiredState` field.** Core 0 writes it (`newState`); a Core 1 coroutine reads it. It is
   a single `int8_t` → the store/load is atomic on the MCU, so at worst a coroutine reads a value
   one loop stale → **benign** (it converges next loop). Left unguarded on purpose (minimal, no
   crash).

Because control only ever writes `desiredState` through `newState`, the concurrency surface is
**exactly these two items** — nothing else. Keeping "the one rule" is what keeps that surface
small enough to reason about.

## Invariants to preserve
- No code outside a `Device` writes pins or the SPI buffer.
- Every control path funnels through `newState` (→ `desiredState`).
- Any new test / diagnostic behaviour is a **coroutine device**, never a side loop.
- The device list is only rebuilt / deleted under `MRJFX_DEVICE_LOCK()`.
- **The effect coroutines never take `MRJFX_DEVICE_LOCK()`** — they must never be starved by the
  network or a reload. This holds only while the reload stays on the Core 1 thread, sequential
  with the scheduler.
