# MrJ-LayoutFX

> **License & attribution — please read before reusing this work.**
> This project is © HO44 PROJECT (MrJ) and licensed under **AGPL-3.0-or-later** (see [LICENSE](LICENSE)).
> If you redistribute, modify, or republish any part of this project — code, PCB design, effects,
> documentation, or the compiled firmware/WebUI — **on this repository or anywhere else** (a fork,
> another platform, a video, a write-up, a product listing), you **must**:
> - keep the original author credit (**MrJ / HO44 PROJECT**) and a link back to
>   [this repository](https://github.com/HO44-PROJECT/MrJ-LayoutFX), and
> - keep the same AGPL-3.0-or-later license on any redistributed or modified version.
>
> Removing or hiding this attribution is not just bad etiquette — under AGPL-3.0 it is a
> **license violation**, and it will be treated as one.
>
> Questions or general discussion: use [GitHub Discussions](https://github.com/HO44-PROJECT/MrJ-LayoutFX/discussions).
> Bug reports and feature requests: use [Issues](https://github.com/HO44-PROJECT/MrJ-LayoutFX/issues).

This project addresses the need for a versatile, DCC-controlled accessory decoder for model railway enthusiasts. The primary objective is to create a compact PCB that interfaces an ESP32 microcontroller with the Digital Command Control (DCC) system, enabling sophisticated control of lighting effects and various peripherals directly from DCC commands.

The design focuses on lighting applications, leveraging the ESP32's computational capabilities to generate realistic lighting effects such as traffic lights, railway signals, beacons, campfires, gas lamps, welding arcs, and many others. However, the board's architecture extends beyond simple LED control, providing standardized interfaces for I2C, UART, and SPI peripherals, making it suitable for controlling servos, sensors, and other intelligent accessories.

## Install

Flash a ready-to-run build straight from your browser — no toolchain, no local server:

**[Install MrJ-LayoutFX](https://ho44-project.github.io/MrJ-LayoutFX/)** (Chrome or Edge, ESP32 connected over USB).

See [web-installer/README.md](web-installer/README.md) for details and a full walkthrough with screenshots.

## Documentation

Start at [doc/](doc/) — organized by who you are (user, advanced, contributing),
with a "Je veux…" index to jump straight to the page you need.

## Related projects

- [MrJ-LayoutFX-ESP32-PCB](https://github.com/HO44-PROJECT/MrJ-LayoutFX-ESP32-PCB) — carrier PCB for the ESP32 DevKitC build.
- [MrJ-LayoutFX-Nano-PCB](https://github.com/HO44-PROJECT/MrJ-LayoutFX-Nano-PCB) — carrier PCB for the Arduino Nano build.
- [MrJ DB-style train signals](https://github.com/HO44-PROJECT/MrJ-HO-scale-DB-style-Era-III-Train-Signals-Electronics) — the HO-scale Deutsche Bahn Era III signal electronics this project grew out of.
