# Device-type reference

[Docs](../README.md) / User guide / Device-type reference

Every device type the WebUI's device editor can create, grouped by
category. **Generated from `data/device_types.json`** by
`tools/build_device_types_doc.py` — run that script after editing the
catalog and commit the result; don't hand-edit the tables below.

See [usage.md](usage.md) for how to open the device editor and use these
types day-to-day.

## Lamps & effects

Static or animated lamp/light effects. On/off, one button in the cockpit.

### Beacon (`Beacon`)

A single flashing warning light.

- **Wires:** 1

### Camp Fire (`CampFire`)

An animated flickering campfire effect.

- **Wires:** 1

### Defect Lamp (`DefectLamp`)

A lamp that occasionally flickers as if faulty.

- **Wires:** 1

### Double Beacon (`DoubleBeacon`)

Two beacons flashing in an alternating pattern.

- **Wires:** 2

### Electric Lamp (`ElectricLamp`)

A steady electric lamp (streetlamp, house light).

- **Wires:** 1

### Gas Lamp (`GasLamp`)

An animated gas lamp with a soft, living flicker.

- **Wires:** 1

### Gas Lamp (defective) (`GasLampDefect`)

A gas lamp with rare, randomised malfunctions layered on the normal flicker.

- **Wires:** 1

### Neon Sign (`NeonSign`)

An animated neon-sign flicker/buzz effect.

- **Wires:** 1

### Oil Lamp (`OilLamp`)

An animated oil-lamp flicker, slower and warmer than a gas lamp.

- **Wires:** 1

### Railway Crossing Lights (`RailwayCrossingLights`)

Alternating level-crossing warning lights.

- **Wires:** 2

### Signal Flare (`SignalFlare`)

A brief, bright flare/burst effect.

- **Wires:** 1

### Solder Lamp (`SolderLamp`)

A lamp effect modelled on a solder/welding-arc flicker.

- **Wires:** 1

### Storm Effect (`Storm`)

A randomised lightning/storm flicker effect, usually paired with other lamps on a layout.

- **Wires:** 1

### Torch (`Torch`)

An animated torch flame, similar to CampFire but tuned for a smaller flame.

- **Wires:** 1

### Train Head Lamp (`TrainHeadLamp`)

A locomotive headlamp effect.

- **Wires:** 1

### Turn Signal (`TurnSignal`)

A blinking turn/indicator signal.

- **Wires:** 1

## Railway signals

Multi-aspect railway signals, charlieplexed over 2–4 wires. One button per aspect in the cockpit.

### DB Bloc Signal (`MrJDBBlocSignal`)

Deutsche Bahn block signal (Hp0/Hp1) — 2 wires, charlieplexed.

- **Wires:** 2
- **States:** `OFF`, `HP0`, `HP1`
- **Wiring assistant:** yes — test each wire and tag it with an aspect (red, green); pins are reordered automatically on save.

### DB Entry Signal (`MrJDBEntrySignal`)

Deutsche Bahn entry signal (Hp0/Hp1/Hp2) — 3 wires, charlieplexed.

- **Wires:** 3
- **States:** `OFF`, `HP0`, `HP1`, `HP2`
- **Wiring assistant:** yes — test each wire and tag it with an aspect (red, green, amber); pins are reordered automatically on save.

### DB Exit Signal (`MrJDBExitSignal`)

Deutsche Bahn exit signal (Hp0/Hp1/Hp2/Hp0+Sh1) — 4 wires, charlieplexed.

- **Wires:** 4
- **States:** `OFF`, `HP00`, `HP1`, `HP2`, `HP0+Sh1`
- **Wiring assistant:** yes — test each wire and tag it with an aspect (amber, red_left, red_right, two_white_green); pins are reordered automatically on save.

## Traffic lights

Road traffic lights. OFF/GO/CAUTION/STOP buttons in the cockpit.

### Traffic Light — 3 Phases (`TrafficLight3ph`)

A 3-phase road traffic light (red/green/amber flashing caution) — 3 wires, charlieplexed.

- **Wires:** 3
- **States:** `OFF`, `STOP`, `GO`, `FLASH`
- **Wiring assistant:** yes — test each wire and tag it with an aspect (red, green, amber); pins are reordered automatically on save.

### Traffic Light — 4 Phases (`TrafficLight4ph`)

A 4-phase road traffic light — 3 wires, charlieplexed, with an extra aspect over the 3-phase version.

- **Wires:** 3
- **States:** `OFF`, `STOP`, `GO`, `FLASH`
- **Wiring assistant:** yes — test each wire and tag it with an aspect (red, green, amber); pins are reordered automatically on save.

## Serial (UART) servos

Serial bus servos (UART). Speed-preset buttons in the cockpit.

### Serial Servo (`SerialServo`)

A Lobot/LX-16A serial bus servo — speed-preset driven (STOP/SLOW/MID/FAST) plus a reverse-direction action.

- **Wires:** 0 (bus device, no GPIO wiring)
- **States:** `STOP`, `SLOW`, `MID`, `FAST`, `REV`

## I²C positional servos

Positional servos on an I²C PWM driver board (PCA9685). One button per configured position.

### PCA9685 Servo (`PCA9685Servo`)

A positional servo on a PCA9685 I²C PWM driver board — moves between named positions (angle, duration, easing) defined per device.

- **Wires:** 1

## I²C motors

Continuous-rotation motors/fans on an I²C PWM driver board (PCA9685). One button per configured speed state.

### PCA9685 Motor (`PCA9685Motor`)

A continuous-rotation motor/fan on a PCA9685 I²C PWM driver board — runs named speed states (speed, ramp up/down, duration) defined per device.

- **Wires:** 1

## Audio

Serial audio playback modules.

### Audio Player (`DfAudio`)

A DFPlayer-style serial audio module — plays sound clips on command.

- **Wires:** 0 (bus device, no GPIO wiring)

## Static outputs

Read-only / always-on outputs with no cockpit control.

### Static (Low) (`StaticLow`)

A plain always-on/off output with no animation — the simplest device kind, read-only in the cockpit.

- **Wires:** variable (1 or more)
