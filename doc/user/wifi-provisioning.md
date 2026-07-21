# WiFi provisioning

[Docs](../README.md) / User guide / WiFi provisioning

How to get a device onto your home WiFi, and how to change the WiFi password
later, without recompiling anything. Credentials are entered from the WebUI
and stored on the device (LittleFS), not baked into the firmware.

## First time — joining your home WiFi

A freshly flashed device has no WiFi credentials, so it boots into its own
access point:

1. Connect your phone/computer to the device's own network (default SSID
   `MrJ-LayoutFX`, password `mrjfx1234`).
2. Browse to `http://192.168.4.1/ui`. You'll see a **WiFi setup** gate screen
   instead of the normal app.
3. Pick your network from the scanned list (or type the SSID by hand if it's
   hidden), enter the password, and click **Connect**.

## What happens when you click Connect

The ESP32 only has **one WiFi radio**, so it cannot keep its own access point
running while it tries to join another network — testing your credentials
means the device must leave its own AP for real. Concretely:

1. The device acknowledges your form submission ("Testing…") and immediately
   drops its own access point.
2. It attempts to join the network you specified, for about 10 seconds.
3. **Your phone loses its connection to the device at this point, regardless
   of whether the credentials are right or wrong.** This isn't a bug — there
   is no way to test a candidate network while keeping the old AP connection
   alive on this hardware (the same approach WLED uses).
4. Reconnect your phone to your **usual home WiFi** and wait a few seconds:
   - **Success** — the device joined your network and rebooted onto it. Check
     the device's **OLED screen** (if fitted) for its new IP address, or check
     your router's client list. Browse to `http://<that-ip>/ui`.
   - **Failure** (wrong password, out of range, etc.) — nothing was saved, so
     the device reboots back into its own access point automatically.
     Reconnect your phone to `MrJ-LayoutFX` and the setup form is still there
     to try again.

There is no separate success/failure popup in the browser — the connection
is gone before the device would be able to send one either way.

## Changing the WiFi password later

If the device is already on your home network and you need to change its
WiFi credentials (new router, changed password…), you can't reach the setup
form through the normal app UI. Instead:

1. **Reset the device twice quickly** (or power-cycle it twice in a row).
   This boots it into **safe mode**: the saved configuration is bypassed (no
   devices load, so a broken config can't interfere) and the device comes up
   as an access point again.
2. Connect to `MrJ-LayoutFX` / `mrjfx1234`, browse to `http://192.168.4.1/ui`,
   and use the same WiFi setup form as above.

Safe mode only affects that boot — a normal reset afterwards loads the saved
configuration again. See also
[Recovery — safe mode](getting-started.md#recovery--safe-mode) in the getting
started guide.

## Compile-time credentials (not recommended)

It's still possible to bake `WIFI_SSID`/`WIFI_PASSWORD` into `config.h` at
build time — see [configuration-flags.md](../advanced/configuration-flags.md).
This is only useful for a single device you build for yourself and never
reflash for someone else; anything you intend to distribute should ship with
no credentials and rely on this runtime flow instead.

## Next

- [getting-started.md](getting-started.md) — first boot, either flashing
  method.
- [usage.md](usage.md) — drive and configure devices from the WebUI.
- [configuration-flags.md](../advanced/configuration-flags.md) — full flag
  reference, including the AP fallback SSID/password.
