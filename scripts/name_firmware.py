"""
name_firmware.py — PlatformIO extra script.

Name the built firmware after the active environment instead of the generic
"firmware", so each board's binary is self-identifying:

    .pio/build/mrj_layoutfx_full/mrj_layoutfx_full.bin

PlatformIO derives the output names (.elf/.bin/.hex) from PROGNAME (default
"firmware"). Overriding it here renames all of them. This avoids picking the
wrong .bin when several environments are built (a real footgun for OTA uploads).

Wired from platformio.ini [env] extra_scripts, so it applies to every env.
"""

Import("env")  # noqa: F821 — PlatformIO global

env.Replace(PROGNAME=env["PIOENV"])  # noqa: F821
