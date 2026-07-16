/**
 * @file DeviceFactoryKeys.h
 * @brief JSON field names and device type strings used by DeviceFactory.
 *
 * Centralises every literal that appears in config.json or board_types.json so
 * that schema renames touch exactly one place.  Consumed only by DeviceFactory.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo    https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author  MrJ
 * @date    2026-04-22
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

namespace factory_keys {

// ── Document-level fields ──────────────────────────────────────────────────
constexpr char kName[] = "name"; ///< Config display name (shown on OLED idle screen).

// ── Document sections ──────────────────────────────────────────────────────
constexpr char kSecBuses[] = "buses";
constexpr char kSecBoards[] = "boards";
constexpr char kSecDevices[] = "devices";
constexpr char kSecIdlePins[] = "idle_pins"; ///< GPIO pins to drive OUTPUT LOW at boot (prevents floating)
constexpr char kSecPins[] = "pins"; ///< board_types.json only

// ── Fields shared across sections ─────────────────────────────────────────
constexpr char kFId[] = "id";
constexpr char kFType[] = "type";
constexpr char kFLabel[] = "label";
constexpr char kFWiring[] = "wiring";
constexpr char kFBus[] = "bus";
constexpr char kFBoard[] = "board";

// ── Bus type values ────────────────────────────────────────────────────────
constexpr char kBusDcc[] = "dcc";
constexpr char kBusSpiMaster[] = "spi_master_only";
constexpr char kBusSpiDuplex[] = "spi_full_duplex";
constexpr char kBusUart[] = "uart";
constexpr char kBusI2c[] = "i2c";

// ── Bus field keys ─────────────────────────────────────────────────────────
constexpr char kFPin[] = "pin";     ///< dcc
constexpr char kFMosi[] = "mosi";   ///< spi_master_only
constexpr char kFSclk[] = "sclk";   ///< spi_master_only
constexpr char kFLatch[] = "latch"; ///< spi_master_only
constexpr char kFTx[] = "tx";       ///< uart
constexpr char kFRx[] = "rx";       ///< uart
constexpr char kFBaud[] = "baud";   ///< uart
constexpr char kFSda[] = "sda";     ///< i2c
constexpr char kFScl[] = "scl";     ///< i2c

// ── Board field keys ──────────────────────────────────────────────────────
constexpr char kFPinCount[] = "pin_count"; ///< Fallback output count for board types unknown to the embedded catalog — the structural count always wins for known types (#54).

// ── Device field keys and values ──────────────────────────────────────────
constexpr char kFAddress[] = "address";
constexpr char kFDefaultState[] = "default_state";
constexpr char kVOn[] = "on";
constexpr char kFStartDelayMs[] = "start_delay_ms";             ///< #8 fixed startup delay before applying default_state ON.
constexpr char kFStartDelayRandomMs[] = "start_delay_random_ms"; ///< #8 additional random startup delay, drawn once at boot/reload.

// ── UART bus key names ────────────────────────────────────────────────────
constexpr char kUartKey0[] = "uart0";
constexpr char kUartKey1[] = "uart1";
constexpr char kUartKey2[] = "uart2";

// ── Device type names — must match the "type" field in "devices[]" ────────
constexpr char kDevBeacon[] = "Beacon";
constexpr char kDevCampFire[] = "CampFire";
constexpr char kDevLed[] = "Led";
constexpr char kDevDefectLamp[] = "DefectLamp";
constexpr char kDevElectricLamp[] = "ElectricLamp";
constexpr char kDevGasLamp[] = "GasLamp";
constexpr char kDevGasLampDefect[] = "GasLampDefect";
constexpr char kDevNeonSign[] = "NeonSign";
constexpr char kDevOilLamp[] = "OilLamp";
constexpr char kDevSignalFlare[] = "SignalFlare";
constexpr char kDevSolderLamp[] = "SolderLamp";
constexpr char kDevStorm[] = "Storm";
constexpr char kDevTorch[] = "Torch";
constexpr char kDevTrainHeadLamp[] = "TrainHeadLamp";
constexpr char kDevTurnSignal[] = "TurnSignal";
constexpr char kDevStaticLow[] = "StaticLow";
constexpr char kDevDoubleBeacon[] = "DoubleBeacon";
constexpr char kDevRailwayCrossing[] = "RailwayCrossingLights";
constexpr char kDevMrJDBBlocSignal[] = "MrJDBBlocSignal";
constexpr char kDevMrJDBEntrySignal[] = "MrJDBEntrySignal";
constexpr char kDevTrafficLight3[] = "TrafficLight3ph";
constexpr char kDevTrafficLight4[] = "TrafficLight4ph";
constexpr char kDevMrJDBExitSignal[] = "MrJDBExitSignal";
constexpr char kDevDfAudio[] = "DfAudio";
constexpr char kDevSerialServo[] = "SerialServo";
constexpr char kDevI2cPwmServo[] = "PCA9685Servo";
constexpr char kDevI2cPwmMotor[] = "PCA9685Motor";

// ── I2C device field keys ─────────────────────────────────────────────────────
constexpr char kFI2cAddress[]   = "i2c_address";
constexpr char kFOscillatorHz[] = "oscillator_hz";
constexpr char kFPositions[]  = "positions";  ///< Array of {angle, duration_ms[, label]} for PCA9685Servo.
constexpr char kFAngle[]      = "angle";       ///< Angle in degrees (0–180) for a servo position.
constexpr char kFDurationMs[] = "duration_ms"; ///< Transition duration in ms for a servo position.
constexpr char kFEaseOut[]    = "ease_out";    ///< Quadratic ease-out on this slew (bool, default false).
constexpr char kFSpeed[]      = "speed";        ///< Motor speed [-100, +100] for PCA9685Motor (backward-compat single-state).
constexpr char kFNeutralUs[]  = "neutral_us";   ///< PWM µs for stop/neutral on PCA9685Motor (default 1500).
constexpr char kFStates[]     = "states";        ///< Array of MotorState objects for PCA9685Motor.
constexpr char kFRampUpMs[]   = "ramp_up_ms";   ///< Ramp-up duration in ms for a MotorState (default 0 = instant).
constexpr char kFRampDownMs[] = "ramp_down_ms"; ///< Ramp-down duration in ms for a MotorState (default 0 = instant).
constexpr char kFPulseMinUs[] = "pulse_min_us"; ///< PWM µs for −90° on PCA9685Servo (default 1000).
constexpr char kFPulseMaxUs[] = "pulse_max_us"; ///< PWM µs for +90° on PCA9685Servo (default 2000).

} // namespace factory_keys
