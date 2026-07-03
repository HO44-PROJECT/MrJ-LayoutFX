# Troubleshooting — investigation logs

Post-mortems of real debugging sessions: what was observed, tested, ruled out and
concluded — kept so the same ground is never covered twice. This section completes
the developer docs: [`../architecture/`](../architecture/) describes how the
firmware *runs*, [`../workshop/`](../workshop/) how it is *built*, and this one
what happened when it *misbehaved*.

Unlike the architecture docs, these are **historical records**, not structural
descriptions: chronology, dead ends, backlog card numbers and hardware photos all
belong here. An entry may be written in French or English — whatever served the
investigation.

## Investigations

- [spi-595-daisy-chain-investigation.md](spi-595-daisy-chain-investigation.md) —
  ✅ résolu — « les sorties 9-16 d'une chaîne 2×74HC595 ne se pilotent pas depuis
  l'UI » : longue enquête logicielle (méthodes de transfert, horloge, cadence de
  rafraîchissement, sous-systèmes coupés un à un)… conclue en panne **matérielle**
  du breadboard, masquée par une baseline périmée (« le POC marche ici ») jamais
  re-vérifiée. Contient la leçon de méthode et la checklist de réparation.

## Writing a new entry

One `.md` per investigation, named after the *symptom*. Recommended shape (see the
SPI entry): a status banner at the top (✅ résolu / 🔍 en cours) with the final
conclusion first, a TL;DR, the setup, a **table of every test with its result**,
what was formally ruled out, the retained explanation, and the lessons learned.
Write it as it happens — the dead ends are the valuable part.
