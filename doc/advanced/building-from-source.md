# Building from source

[Docs](../README.md) / Advanced / Building from source

Use this instead of the [browser flasher](../user/getting-started.md) when you
need a different board (e.g. an AVR Nano) or a hardware feature outside the
generic browser build (a specific bus mix, DCC, OLED…). WiFi credentials don't
require a custom build either way — they're set at runtime, see
[wifi-provisioning.md](../user/wifi-provisioning.md).

## Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).
- An **ESP32** board for the full feature set (WiFi/WebUI/OTA). AVR boards run a
  reduced GPIO-only subset.
- A USB cable for the first flash.

## 1. The sketch

A user `main.cpp` is tiny — the library does the work:

```c
#include <LayoutFX.h>
#include "config.h"

void setup() { LayoutFX::init(); }
void loop()  { LayoutFX::loop(); }
```

## 2. `config.h` — compile-time features

`config.h` selects which features are **built in**. A typical ESP32 setup:

```c
#pragma once

#define CONFIG          "config.json"   // device config on LittleFS
#define WEBUI                           // web control panel (implies API)
#define OTA                             // wireless updates afterwards
#define LOG_SERIAL                      // boot/operational logs on USB serial
```

Add hardware flags as needed (`SPI_CARDS`, `I2C_CARDS`, `DCC_PIN`, `OLED`…).
Every flag, with its default, is in
[configuration-flags.md](configuration-flags.md).

> `WEBUI`/`API` need `CONFIG`. WiFi credentials are **not** set here — leave
> `WIFI_SSID`/`WIFI_PASSWORD` out entirely (recommended for anything you'll
> distribute) and the device boots into its own SoftAP (`WIFI_AP_SSID` /
> `WIFI_AP_PASSWORD`, defaults `MrJ-LayoutFX` / `mrjfx1234`), provisioned at
> runtime from the WebUI's WiFi form — see
> [wifi-provisioning.md](../user/wifi-provisioning.md). The two `#define`s
> still exist for anyone who wants credentials baked in at compile time
> instead, but that's no longer the recommended path.

## 3. Build & upload

Two things live on the device: the **firmware** and the **filesystem** (LittleFS),
which holds `config.json` and the WebUI data.

```sh
# firmware
pio run -e <your-env> -t upload
# filesystem (config.json + data/) — needed at least once
pio run -e <your-env> -t uploadfs
```

> The WebUI page, device/board/bus catalogs and feature badges are **embedded in
> the firmware** (PROGMEM). After changing them you must re-flash the firmware and
> hard-reload the browser. `config.json` lives on the filesystem and can be edited
> live from the WebUI.

## 4. First boot

1. Open a serial monitor at **115200 baud**. The boot prints structural logs
   (banner, then the WiFi **IP address**). Note the IP.
2. Browse to `http://<ip>/ui`.
   - If the device started in SoftAP mode, connect to the `MrJ-LayoutFX`
     network first, then open `http://192.168.4.1/ui`.
3. The cockpit is empty until you add devices — go to the **Configuration** tab to
   declare your boards, buses and devices, or upload a `config.json`.

## Next

- [../user/usage.md](../user/usage.md) — drive and configure devices from the WebUI.
- [configuration-flags.md](configuration-flags.md) — full flag reference.
- [../contributing/](../contributing/) — project conventions, build pipeline,
  how the firmware is put together, if you're going to change the code itself
  rather than just its configuration.
