# Getting started

Two ways to get a device running, from easiest to most flexible.

## Option A — browser flash (no install, no PlatformIO)

Like WLED: open a web page, plug the ESP32 in over USB, click *Install*. Uses
[ESP Web Tools](https://esphome.github.io/esp-web-tools/) (Web Serial), works in
**Chrome or Edge on desktop** only (Web Serial isn't available in Firefox,
Safari, or on mobile — those show a notice instead of the install button).

1. Open the web installer page (ask the project maintainer for the current URL,
   or run it locally — see
   [web-installer/README.md](../../web-installer/README.md) if you're building
   from a checkout).
2. Plug in the ESP32, click **Install**, wait for it to finish.
3. The board reboots with an **empty configuration** and starts its own WiFi
   access point (default SSID `MrJ-RailwayFX`, password `mrjfx1234`) since it
   has no home network to join yet.
4. Connect your computer/phone to that access point, browse to
   `http://192.168.4.1/ui`.
5. Go to the **Configuration** tab and either run the setup wizard or declare
   your boards/buses/devices by hand — see [usage.md](usage.md).

This flashes firmware only, no layout — you always configure the layout
afterwards from the UI, which is normal, not a sign anything went wrong.

To join your **home WiFi** instead of staying on the access point, you'll need
a build with your SSID/password baked in (`WIFI_SSID`/`WIFI_PASSWORD`), which
means [building from source](../advanced/building-from-source.md) — the browser
flasher currently ships one generic, network-agnostic firmware.

## Option B — build from source

For your own WiFi credentials, a different board (AVR/Nano), or any hardware
feature not in the generic browser build (specific bus combination, DCC, OLED,
etc.). See [building-from-source.md](../advanced/building-from-source.md).

## First boot, either way

1. If you flashed with WiFi credentials and the device joined your network,
   find its IP (router's client list, or a serial monitor at 115200 baud shows
   it on boot) and browse to `http://<ip>/ui`.
2. If it's on its own access point instead (no WiFi configured, or it
   couldn't join), connect to that network first, then browse to
   `http://192.168.4.1/ui`.
3. The **Cockpit** is empty until you add devices — go to **Configuration** to
   declare your boards, buses and devices (by hand or via the setup wizard), or
   upload an existing `config.json`. See [usage.md](usage.md).

## Recovery — safe mode

If a saved configuration makes the device crash or become unreachable,
**reset twice quickly** (or power-cycle twice). The device boots into a
**safe mode**: the configuration is bypassed (no devices load) and it comes up
as an access point, so the WebUI is always reachable to fix or delete the
config. This is for that session only — the next normal boot loads the
configuration again.

## Next

- [usage.md](usage.md) — drive and configure devices from the WebUI.
- [building-from-source.md](../advanced/building-from-source.md) — custom
  firmware, your own WiFi credentials, every hardware flag.
