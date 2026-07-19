# Configuration flags reference

Every behaviour of the firmware is driven by `#define`s in your **`config.h`**
(force-included at build time). This page lists **all** user-settable flags,
their type, default, and effect.

The canonical, in-source copy of this list lives in
[`include/LayoutFX_define.h`](../../include/LayoutFX_define.h) (flags →
`LFX_*_ENABLED`) and [`include/LayoutFX_default.h`](../../include/LayoutFX_default.h)
(default values). The WebUI **feature badges** (About → Features) mirror these
flags: a badge is green when the flag is *compiled in* ("built"), not necessarily
*active* at runtime.

Conventions:
- **Toggle** = define it (any/no value) to enable; **default off** = leave it out.
- **Value** = define it with a value; the **Default** column is used when omitted.
- A few flags need ESP32 (WiFi, WebUI, OTA, config, I²C) — noted where relevant.

---

## Core / config / network (ESP32)

| Flag | Type | Default | Effect |
|---|---|---|---|
| `CONFIG` | `"file.json"` | — | Load the device config from LittleFS (filename, no leading `/`). Required for the WebUI/API to control anything. |
| `API` | toggle | off | REST API server (`/api/…`). Requires WiFi **and** `CONFIG`. |
| `WEBUI` | toggle | off | Web control panel (`/ui`). Implies `API`. Requires WiFi **and** `CONFIG`. |
| `WIFI_SSID` | `"…"` | — | STA SSID. **Both** SSID and password are required to join WiFi and start the HTTP server. |
| `WIFI_PASSWORD` | `"…"` | — | STA password. |
| `WIFI_AP_SSID` | `"…"` | `"MrJ-RailwayFX"` | Access-point fallback SSID. |
| `WIFI_AP_PASSWORD` | `"…"` | `"mrjfx1234"` | AP fallback password (min 8 chars, or `""` for an open network). |
| `WIFI_FORCE_AP` | toggle | off | Skip STA entirely and boot straight into access-point mode. |
| `HTTP_PORT` | `<n>` | `80` | HTTP server port. |
| `OTA` | toggle | off | Wireless firmware update: espota (`pio run -t upload`) **and** the web `/update` uploader. |
| `OTA_HOSTNAME` | `"…"` | `"mrjfx"` | mDNS base name; a MAC suffix is appended → `mrjfx-xxxx.local`. |
| `OTA_PASSWORD` | `"…"` | — (none) | Auth for espota and the web uploader. Recommended on a shared network. |

## Buses / hardware

| Flag | Type | Default | Effect |
|---|---|---|---|
| `DCC_PIN` | `<n>` | — (DCC off) | Enable the NMRA DCC decoder on GPIO `<n>`. |
| `DCC_AUDIT` | toggle | off | Log every received DCC packet (diagnostics). |
| `SPI_CARDS` | toggle | off | 74HC595 SPI shift-register bus (chained digital outputs). |
| `I2C_CARDS` | toggle | off | I²C device drivers (PCA9685 servo boards…). Brings up the I²C bus. |
| `I2C_SCAN` | toggle | off | Expose `GET /api/scan/i2c`. Also brings up the I²C bus. |
| `I2C_SDA` / `I2C_SCL` | `<n>` | `21` / `22` | I²C bus pins (ESP32 hardware defaults). |
| `LOBOT` | toggle | off | Lobot LX-16A serial-servo protocol (implies `LX16A`). |
| `LX16A` | toggle | off | LX-16A serial servo without the full Lobot stack. |
| `AUDIO` | toggle | off | DFPlayer-style serial audio device support. |
| `USE_JTAG` | toggle | off | Do **not** drive GPIO 5/10/12-15 LOW at boot (keep a JTAG probe usable). |

## OLED display

| Flag | Type | Default | Effect |
|---|---|---|---|
| `OLED` | toggle | off | SSD1306 OLED over I²C. Brings up the I²C bus. |
| `OLED_STATUS` | toggle | off | Lightweight status screen via `StatusOled` (also works on AVR). |
| `OLED_SPLASH` | toggle | off | Show a boot splash screen. |
| `OLED_CONTRAST` | `<n>` 0-255 | display default (not applied) | Display contrast. |
| `OLED_FLIP_MODE` | `<n>` | display default (no flip) | Rotation / flip mode. |
| `OLED_HEIGHT` | `64` / `32` | `64` | Panel height in px — `64` (0.96") or `32` (0.91"). |
| `OLED_SDA` / `OLED_SCL` | `<n>` | `= I2C_SDA` / `I2C_SCL` | Put the OLED on a separate bus (address clash / different wiring). |
| `OLED_EVENT_MS` | `<n>` | `3000` | How long (ms) the event screen shows before returning to idle. |
| `OLED_DEBUG_METRICS` | toggle | off | Show runtime metrics (heap, uptime…) on the OLED. |
| `OLED_DEBUG_EVENTS` | toggle | off | Show event traces on the OLED. |

## Logging — three serial situations + two OLED sinks

These are **independent** and not mutually exclusive.

| Flag | Type | Default | Effect |
|---|---|---|---|
| *(boot default)* | — | **always on** | `Serial.begin()` at boot prints **structural Tier-1** logs (banner, IP, config result). Not toggleable via `config.h`. |
| `LOG_SERIAL` | toggle | off | **Operational Tier-2** logs on UART0 (`LOG_PRINT…`). Runtime-gated by the `uart0` bus — remove it to free GPIO1/3. |
| `DEBUG_SERIAL` | toggle | off | **Verbose debug** logs on UART0 (`DEBUG_PRINT…`). |
| `LOG_OLED` | toggle | off | Mirror operational logs to the OLED. |
| `DEBUG_OLED` | toggle | off | Send debug logs to the OLED instead of serial. |

## Behaviour

| Flag | Type | Default | Effect |
|---|---|---|---|
| `DEMO` | toggle | off | Built-in demo sequences (traffic, signals, servo, LED effects). |

## Capacity limits

Override only if you hit a ceiling; larger values use more RAM.

| Flag | Default | | Flag | Default |
|---|---|---|---|---|
| `BUS_MAX_SPI_CARDS` | `8` | | `FACTORY_MAX_DEVICES` | `256` |
| `BUS_MAX_UART` | `4` | | `FACTORY_MAX_BOARDS` | `12` |
| `BUS_MAX_I2C` | `2` | | `FACTORY_MAX_BUSES` | `8` |
| | | | `FACTORY_MAX_PORTS` | `4` |
| | | | `FACTORY_MAX_BOARD_TYPES` | `16` |
| | | | `FACTORY_MAX_SPI_CARDS` | `8` |

---

## Example `config.h`

```c
#pragma once

// Networking + control
#define CONFIG          "config.json"   // device config on LittleFS
#define WIFI_SSID       "my-network"
#define WIFI_PASSWORD   "my-password"
#define WEBUI                           // implies API
#define OTA                             // wireless updates

// Hardware
#define SPI_CARDS                       // 74HC595 chain
#define I2C_CARDS                       // PCA9685 servo boards
#define DCC_PIN         34              // DCC decoder on GPIO34

// Diagnostics
#define LOG_SERIAL                      // operational logs on UART0
```

See [building-from-source.md](building-from-source.md) for the full first-build
walkthrough.
