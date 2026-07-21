# JSON Schemas

JSON Schema (Draft 2020-12) definitions for this project's config and data files.

| Schema | Validates |
|---|---|
| [`config.schema.json`](config.schema.json) | `config.json` — devices, boards, buses, DCC mapping. |
| [`device_types.schema.json`](device_types.schema.json) | `device_types.json` — the device-type catalog (wiring, states, defaults). |
| [`board_types.schema.json`](board_types.schema.json) | `board_types.json` — expansion board catalog (PCA9685, MCP23017, …). |
| [`bus_types.schema.json`](bus_types.schema.json) | `bus_types.json` — bus types (I2C, SPI) and their addressing modes. |
| [`i2c_known.schema.json`](i2c_known.schema.json) | `i2c_known.json` — known I2C chips (addresses, names, typical use). |

## Editor validation

VS Code applies these automatically via `.vscode/settings.json`'s `json.schemas`
entries, or via the `$schema` key at the top of each JSON file — open a `config.json`
and errors/autocomplete show up inline, no setup needed beyond that.
