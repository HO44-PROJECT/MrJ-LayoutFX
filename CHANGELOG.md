# Changelog

Notable changes to MrJ-RailwayFX, newest first. Each entry links the backlog
issue(s) it closes. Started 2026-07-11 — earlier history lives in `git log`.

## Unreleased

### Added
- Device and board types now have a human-readable `label` (short, shown once
  placed) and an optional `dropdownLabel` (longer, shown only in the
  selection dropdown — carries author credit for the custom PCBs and the
  MrJDB signal family without repeating it on every card). (#77)

### Fixed
- Device editor: the type-picker's open panel could show two scrollbars at
  once (the panel's own list plus the modal body) once the option list grew
  past ~6 entries. (#77)
- The 3 MrJDB signal types (`MrJDBBlocSignal`, `MrJDBEntrySignal`,
  `MrJDBExitSignal`) used two visibly different reds across their wire-aspect
  swatches (`#c0392b` vs `#e74c3c`), and the unified red still read too close
  to the amber swatch. All red wire-aspects now use a single `#e60000`. (#77)
