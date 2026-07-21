"""
copy_to_dist.py — PlatformIO post-build script.

Copies the built firmware (.bin) into dist/<env>/ after a successful build,
so the flashable deliverables live in one predictable, gitignored place
instead of buried under .pio/build/<env>/ — handy for grabbing a binary to
flash elsewhere, or for a future GitHub release step.

.pio/build/<env>/ keeps every intermediate build artifact as usual; dist/
only ever holds the final outputs, one per env.

Wired from platformio.ini [env] extra_scripts, so it applies to every env.
"""

import shutil
from pathlib import Path

Import("env")  # noqa: F821 — PlatformIO global


def _copy_to_dist(source, target, env):  # noqa: ARG001 — PlatformIO callback signature
    pioenv = env["PIOENV"]
    build_dir = Path(env.subst("$BUILD_DIR"))
    dist_dir = Path(env["PROJECT_DIR"]) / "dist" / pioenv
    dist_dir.mkdir(parents=True, exist_ok=True)

    src = build_dir / f"{pioenv}.bin"
    if src.exists():
        shutil.copy2(src, dist_dir / src.name)

    print(f"[copy_to_dist] {pioenv} → dist/{pioenv}/")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _copy_to_dist)  # noqa: F821
