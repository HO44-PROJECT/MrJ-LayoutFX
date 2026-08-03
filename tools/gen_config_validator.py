"""
@file    gen_config_validator.py
@brief   PlatformIO pre-build script: compile schemas/config.schema.json into a
         dependency-free JS validator, embedded in the WebUI bundle so the
         config-upload path (app-config.js uploadConfig()) can reject a
         malformed config.json client-side, against the real schema, before
         it ever reaches the ESP32.

@details
  Uses Ajv (https://ajv.js.org/) in "standalone code generation" mode: Ajv
  itself only runs here, at build time, as Node/npm dev-tooling (same role as
  rjsmin/rcssmin in build_webui.py) — the *output* is a plain, self-contained
  JS file with zero runtime dependency on Ajv, safe to gzip into firmware
  PROGMEM like any other WebUI module.

  Opt-out: define NO_CONFIG_SCHEMA_VALIDATION in the environment's config.h to
  skip this entirely (no Node/npm required, no validator embedded, upload
  falls back to the pre-existing JSON.parse()-only check).

  Requires Node.js + npm on the machine doing the build. If either is missing,
  this script prints a warning and skips generation rather than failing the
  build — the feature is additive, not load-bearing.

  Output:
    src/web/generated/validate_config.js — plain-global JS (var validateConfig
    = ...; no module system, matching every other WebUI script), added to
    build_webui.py's APP_MODULES list when present.

@project MrJ-LayoutFX
@license AGPL-3.0-or-later — Copyright (c) 2026 HO44 PROJECT
"""

Import("env")  # noqa: F821

import re
import shutil
import subprocess
from pathlib import Path


def _library_root() -> Path:
    project_dir = Path(env.get("PROJECT_DIR"))  # noqa: F821
    if (project_dir / "tools" / "gen_config_validator.py").exists():
        return project_dir
    for candidate in (project_dir / "lib").glob("*"):
        if (candidate / "tools" / "gen_config_validator.py").exists():
            return candidate
    raise RuntimeError(f"Could not locate library root from PROJECT_DIR={project_dir}")


_LIB = _library_root()
_SCHEMA = _LIB / "schemas" / "config.schema.json"
_OUT_DIR = _LIB / "src" / "web" / "generated"
_OUT = _OUT_DIR / "validate_config.js"
_GEN_SCRIPT = _LIB / "tools" / "_ajv_gen.mjs"


def _config_h_defines(flag: str) -> bool:
    """Checks whether `#define <flag>` (uncommented) appears in the active
    env's config.h — same resolution PlatformIO uses for -include, read via
    GetProjectOption so extends/${...}/custom_cfg all resolve exactly like
    the real compile does (see gen_build_info.py for the same pattern)."""
    custom_cfg = env.subst(env.GetProjectOption("custom_cfg", "$PIOENV"))  # noqa: F821
    hdr = Path(env.subst("$PROJECT_DIR")) / "configurations" / custom_cfg / "config.h"  # noqa: F821
    if not hdr.exists():
        return False
    text = hdr.read_text(encoding="utf-8")
    return re.search(rf"^\s*#define\s+{re.escape(flag)}\b", text, re.MULTILINE) is not None


if _config_h_defines("NO_CONFIG_SCHEMA_VALIDATION"):
    print("[gen_config_validator] NO_CONFIG_SCHEMA_VALIDATION defined — skipping, upload will only JSON.parse()-check.")
    _OUT.unlink(missing_ok=True)
else:
    node = shutil.which("node")
    npm = shutil.which("npm")
    if not node or not npm:
        print("[gen_config_validator] node/npm not found on PATH — skipping (install Node.js to enable client-side schema validation, or add #define NO_CONFIG_SCHEMA_VALIDATION to config.h to silence this).")
        _OUT.unlink(missing_ok=True)
    else:
        _TOOLS = _LIB / "tools"
        if not (_TOOLS / "node_modules" / "ajv").exists():
            print("[gen_config_validator] Installing ajv (build-time only, not shipped in firmware)...")
            try:
                subprocess.run([npm, "install", "--no-audit", "--no-fund"], cwd=str(_TOOLS), check=True)
            except subprocess.CalledProcessError as e:
                print(f"[gen_config_validator] npm install failed ({e}) — skipping, upload will only JSON.parse()-check.")
                _OUT.unlink(missing_ok=True)
                node = None  # skip the generation step below

        if node:
            _OUT_DIR.mkdir(parents=True, exist_ok=True)
            try:
                subprocess.run(
                    [node, str(_GEN_SCRIPT), str(_SCHEMA), str(_OUT)],
                    cwd=str(_TOOLS),
                    check=True,
                )
                size = _OUT.stat().st_size
                print(f"[gen_config_validator] {_OUT.relative_to(_LIB)} ({size} B)")
            except subprocess.CalledProcessError as e:
                print(f"[gen_config_validator] generation failed ({e}) — skipping, upload will only JSON.parse()-check.")
                _OUT.unlink(missing_ok=True)
