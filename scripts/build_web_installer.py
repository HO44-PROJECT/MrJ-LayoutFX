#!/usr/bin/env python3
"""Assemble web-installer/ for ESP Web Tools (browser USB flashing, à la WLED).

Collects the four ESP32 flash parts produced by `pio run -e <env>` and writes a
`manifest.json` that the `<esp-web-install-button>` on web-installer/index.html
consumes. The firmware is self-contained (WebUI + catalogs in PROGMEM), so a fresh
flash boots a working WebUI with an empty config — no filesystem image needed.

Usage:
    pio run -e web_installer
    python3 scripts/build_web_installer.py [--env <env>] [--version vX]
    cd web-installer && python3 -m http.server 8000   # open in Chrome/Edge

Defaults to the `web_installer` env, which is deliberately WiFi-agnostic (no
configurations/auth/wifi.h). Don't pass --env esp32devkitc_breadboard here —
that config bakes in the maintainer's own WiFi credentials and must never be
published as a downloadable binary.

Classic ESP32 (esp32dev) flash layout — offsets in bytes:
    0x1000  bootloader.bin
    0x8000  partitions.bin
    0xe000  boot_app0.bin   (from the Arduino-ESP32 framework package)
    0x10000 <app>.bin       (named after the env by scripts/name_firmware.py)
"""
import argparse, configparser, glob, json, os, re, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "web-installer")

# (output filename, flash offset) — app source filename is resolved at runtime.
PARTS = [
    ("bootloader.bin", 0x1000),
    ("partitions.bin", 0x8000),
    ("boot_app0.bin", 0xE000),
    ("app.bin", 0x10000),
]


def build_dir():
    """Resolve the PlatformIO build_dir (env override → platformio.ini → default)."""
    env = os.environ.get("PLATFORMIO_BUILD_DIR")
    if env:
        return env
    ini = configparser.ConfigParser(inline_comment_prefixes=(";",))
    ini.read(os.path.join(ROOT, "platformio.ini"))
    if ini.has_option("platformio", "build_dir"):
        return os.path.expanduser(ini.get("platformio", "build_dir"))
    return os.path.join(ROOT, ".pio", "build")


def firmware_version():
    try:
        h = open(os.path.join(ROOT, "include/LayoutFX_default.h")).read()
        m = re.search(r'LFX_FIRMWARE_VERSION\s+"([^"]+)"', h)
        if m:
            return m.group(1)
    except OSError:
        pass
    return "dev"


def main():
    ap = argparse.ArgumentParser(description="Assemble web-installer/ for ESP Web Tools.")
    ap.add_argument("--env", default="web_installer")
    ap.add_argument("--version", default=None)
    args = ap.parse_args()

    src = os.path.join(build_dir(), args.env)
    if not os.path.isdir(src):
        sys.exit("Build dir not found: %s\n  Run `pio run -e %s` first." % (src, args.env))
    os.makedirs(OUT, exist_ok=True)

    # App binary: <env>.bin (PROGNAME) or firmware.bin (default).
    app = next((c for c in (args.env + ".bin", "firmware.bin")
                if os.path.isfile(os.path.join(src, c))), None)
    if not app:
        sys.exit("App binary not found in %s" % src)

    # boot_app0.bin ships with the Arduino-ESP32 framework package.
    boots = sorted(glob.glob(os.path.expanduser(
        "~/.platformio/packages/framework-arduinoespressif32*/tools/partitions/boot_app0.bin")))
    if not boots:
        sys.exit("boot_app0.bin not found — is the espressif32 framework installed?")

    sources = {
        "bootloader.bin": os.path.join(src, "bootloader.bin"),
        "partitions.bin": os.path.join(src, "partitions.bin"),
        "boot_app0.bin": boots[-1],
        "app.bin": os.path.join(src, app),
    }
    parts = []
    for name, offset in PARTS:
        s = sources[name]
        if not os.path.isfile(s):
            sys.exit("Missing required binary: %s" % s)
        shutil.copy2(s, os.path.join(OUT, name))
        parts.append({"path": name, "offset": offset})

    version = args.version or firmware_version()
    manifest = {
        "name": "MrJ-LayoutFX",
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [{"chipFamily": "ESP32", "parts": parts}],
    }
    with open(os.path.join(OUT, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    print("web-installer/ ready  (version %s, env %s, app %s)" % (version, args.env, app))
    print("Test locally:  cd web-installer && python3 -m http.server 8000")
    print("Then open http://localhost:8000 in Chrome or Edge, USB-connect the ESP32, Install.")


if __name__ == "__main__":
    main()
