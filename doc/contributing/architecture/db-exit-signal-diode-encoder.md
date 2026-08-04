# DB Exit Signal — diode-encoder truth table

[Docs](../../README.md) / [Contributing](../README.md) / [Architecture](README.md) / DB Exit Signal diode encoder

Hardware reference for the `MrJDBExitSignal` device (see
[device-types.md](../../user/device-types.md#db-exit-signal-mrjdbexitsignal)):
the charlieplexed circuit uses a diode network to encode 4 logical inputs
(Hp0, Hp1, Hp2, Sh1) onto 4 output lines (J1-J4).

## Principle

Wired logic, passive (diodes only, no active components): each active input
forces certain outputs **L** (Low = line active) via diodes; unforced outputs
stay **H** (High = inactive). TTL levels throughout.

**Constraint:** Hp0 and Sh1 are always active together — there is no case
where either is active alone, so the table only needs to cover the
combinations that can actually occur.

## Truth table

| Active input | J1 | J2 | J3 | J4 |
|---------------|----|----|----|----|
| Hp0 + Sh1     | L  | H  | H  | L  |
| Hp00          | H  | L  | H  | H  |
| Hp1           | H  | L  | H  | H  |
| Hp2           | H  | L  | L  | H  |

- **Hp1** activates only J2.
- **Hp2** activates J2 and J3.
- **Hp0 + Sh1** activates only J1.

Each state produces a unique signature across the 4 lines.

## Electrical notes

- Series resistors (220 Ω) limit current.
- TTL levels must be respected to avoid indeterminate states.
- Diodes prevent current backflow between lines.
