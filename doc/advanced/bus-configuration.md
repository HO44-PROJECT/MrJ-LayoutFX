# Bus configuration principles

[Docs](../README.md) / Advanced / Bus configuration principles

## The two configuration layers

The firmware distinguishes two configuration levels that coexist without overlapping:

| Layer | Mechanism | Nature | Modifiable without recompiling |
|--------|-----------|--------|---------------------------|
| **Structural** | `#define` in `config.h` | Hardwired components (soldered, fixed) | No |
| **Application** | `buses` section in `config.json` | Peripherals on connectors | Yes |

**Fundamental rule**: a hardware resource declared via `#define` is implicitly reserved and does not appear in `config.json`. The two layers are orthogonal.

---

## I²C

### Structural OLED

The OLED screen is a structural component of the board — it is soldered once and for all. It is configured at compile time:

```c
#define OLED          // enables the feature
#define OLED_SDA 21   // SDA pin (ESP32 default: 21)
#define OLED_SCL 22   // SCL pin (ESP32 default: 22)
```

If the OLED is wired to non-standard pins, `OLED_SDA` and `OLED_SCL` must be redefined in `config.h`. This is the responsibility of the board designer.

The U8G2 driver (`HW_I2C` mode) initializes `Wire` (I²C0) on these pins at boot. **`Wire` is therefore owned by the OLED** as soon as `#define OLED` is active.

### Application I²C bus

An I²C bus for connectable peripherals is declared in `config.json`:

```json
"buses": {
  "i2c_main": { "type": "i2c", "sda": 4, "scl": 5 }
}
```

The firmware automatically resolves which hardware controller to use:

| Situation | Assigned controller |
|-----------|-------------------|
| `OLED` not enabled | `Wire` (I²C0) |
| `OLED` enabled, pins identical to `OLED_SDA`/`OLED_SCL` | Shared `Wire` — no additional `begin()` |
| `OLED` enabled, different pins | `Wire1` (I²C1) |

The user never manipulates `Wire` vs `Wire1` directly. They declare pins, the firmware chooses.

### Limits

- **OLED enabled**: only 1 application I²C bus possible (`Wire1` is the last free controller).
- **OLED disabled**: 2 I²C buses max (`Wire` + `Wire1`).

---

## UART

### Structural debug port

`LOG_SERIAL` and/or `MRJ_DEBUG_SERIAL` enabled in `config.h` implicitly reserve `uart0` (GPIO 1 = TX, GPIO 3 = RX) — the ESP32's USB/programming port. These pins are fixed by the ESP32 hardware; there is nothing to configure.

```c
#define LOG_SERIAL    // reserves uart0 (GPIO 1/3) — implicit
```

### Application UART buses

Serial buses for peripherals are declared in `config.json`. The bus key must match the name of the hardware serial port:

```json
"buses": {
  "uart1": { "type": "uart", "tx": 17, "rx": 16, "baud": 115200 },
  "uart2": { "type": "uart", "tx": 25, "rx": 26, "baud":   9600 }
}
```

| Bus key | Arduino object | Available if |
|---------|--------------|---------------|
| `uart0` | `Serial`  | `LOG_SERIAL` / `MRJ_DEBUG_SERIAL` not enabled |
| `uart1` | `Serial1` | Always available |
| `uart2` | `Serial2` | Always available |

TX/RX pins are freely chosen on any GPIO via the ESP32 GPIO matrix.

**Limit**: 3 UART buses max, with `uart0` generally reserved for debug.

---

## SPI

### A single SPI bus by design

The project supports **a single SPI bus** (output-only master, for daisy-chained 74HC595s). This is a deliberate design choice: adding a second SPI bus would introduce controller-management complexity (VSPI/HSPI) with no justified use case in this project.

The SPI bus is enabled at compile time:

```c
#define SPI_CARDS     // enables 74HC595 support
```

Then configured in `config.json`:

```json
"buses": {
  "spi_out": { "type": "spi_master_only", "mosi": 23, "sclk": 18, "latch": 5 }
}
```

The MOSI, SCLK and LATCH pins are freely chosen on any GPIO. Without `#define SPI_CARDS`, the `spi_master_only` entry is ignored during parsing.

---

## Summary — reserved vs configurable pins

| Resource | Reserved by | Visible in `config.json` | Exposed in `/api/status` |
|-----------|-------------|---------------------------|---------------------------|
| OLED SDA/SCL | `#define OLED` + `OLED_SDA`/`OLED_SCL` | No | Yes (`sys_pins`) |
| UART0 TX/RX | `#define LOG_SERIAL` / `MRJ_DEBUG_SERIAL` | No | Yes (`sys_pins`) |
| Application UART bus | — | Yes (`uart1`, `uart2`) | Via `buses` |
| Application I²C bus | — | Yes | Via `buses` |
| SPI bus (HC595) | `#define SPI_CARDS` | Yes (`spi_master_only`) | Via `features.spi` |

`/api/status` exposes `sys_pins` (reserved structural pins) and `features` (active compile-time flags) so the UI can gray out pins that are unavailable in the configurator.
