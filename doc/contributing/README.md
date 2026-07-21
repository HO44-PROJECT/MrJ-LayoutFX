# Development workshop

Developer-facing notes on **how the firmware is built** — the toolchain, the
code-generation pipeline, and the project conventions that keep the whole thing
consistent. This is the counterpart to [`architecture/`](architecture/),
which describes how the firmware *runs*; here we describe how it is *produced*.

- [platformio.md](platformio.md) — the PlatformIO project model: the off-Drive
  build directory, template sections vs. buildable environments, `extends`, how
  each environment selects its own `main.cpp` and injects its `config.h`, and the
  `extra_scripts` merge rule that wires the pre-build hooks.
- [build-pipeline.md](build-pipeline.md) — the three pre-build code generators
  (`build_embedded_data.py`, `build_webui.py`, `gen_build_info.py`) that turn
  `data/*.json` and `src/web/*` into gzipped PROGMEM headers under
  `include/generated/`, why they are reproducible, and what a change requires
  (reflash vs. filesystem upload).
- [conventions.md](conventions.md) — the project conventions: the two-repository
  layout, the source tree (`src/` / `include/` / `data/` / `src/web/`), generated
  files are never edited or committed, the coroutine authoring rules (never block,
  state in members), i18n by design, and the catalog/flag consistency rules that
  span several files.

_Add one focused `.md` per workshop topic here._
