# Software architecture

Developer-facing notes on how the library is built (distinct from the user docs in `doc/`).

- [concurrency.md](concurrency.md) — the control model (UI / API / DCC → `newState` only, never
  raw pin access) and how bi-core concurrency is handled (the single device-list mutex, the benign
  `desiredState` field access).

_Add one focused `.md` per architectural topic here._
