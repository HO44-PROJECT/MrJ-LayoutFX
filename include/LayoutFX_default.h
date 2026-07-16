/**
 * @file LayoutFX_default.h
 * @brief General configuration for the MrJ-ArduinoRailwayFX project.
 *
 * This header file defines configuration constants and preprocessor directives for the
 * MrJ-ArduinoRailwayFX project. It includes settings for DCC message decoding, OLED display,
 * debug output, servo support, and various effects such as campfire, gas lamp, traffic lights,
 * and charlieplexing signals. The constants are used to control timing, intensity, and behavior
 * of various devices and effects in railway signaling applications.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-17
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

// ── Project identity ──────────────────────────────────────────────────────────
// SINGLE SOURCE OF TRUTH for the displayed brand name. Used by all firmware
// display strings (OLED, AP SSID default, API messages) AND injected into the
// WebUI bundle and the embedded JSON catalogs at build time: build_webui.py and
// build_embedded_data.py parse this line and substitute every %%BRAND%% token.
// Rebranding the project display name = editing this one line.
#define LFX_PROJECT_NAME "LayoutFX"

// ── Firmware version ──────────────────────────────────────────────────────────
#define LFX_FIRMWARE_VERSION "v1.0"

// Effects parameters

// JMRI constants for DB signals
#define DB_SIGNAL_ASPECT_ID_HP0 0U     ///< Signal state HP0 (Red) or HP00 (Double Red)
#define DB_SIGNAL_ASPECT_ID_HP1 1U     ///< Signal state HP1 (Green)
#define DB_SIGNAL_ASPECT_ID_HP2 2U     ///< Signal state HP2 (Moon)
#define DB_SIGNAL_ASPECT_ID_HP0_SH1 3U ///< Signal state HP0 + SH1 (Moon)
#define DB_SIGNAL_ASPECT_ID_UNLIT 9U   ///< Unlit state for signals (demo mode)

// JMRI constants for Lamp states
#define LAMP_ASPECT_ID_OFF 0U ///< Lamp off state
#define LAMP_ASPECT_ID_ON 1U  ///< Lamp on state

// JMRI constants for Traffic light states
#define TRAFFIC_LIGHT_ASPECT_ID_STOP 0U     ///< Traffic light stop state (Red)
#define TRAFFIC_LIGHT_ASPECT_ID_GO 1U       ///< Traffic light go state (Green)
#define TRAFFIC_LIGHT_ASPECT_ID_FLASHING 2U ///< Traffic light flashing state (Amber)

// Campfire effect configuration constants
#define CAMPFIRE_BASE_INTENSITY 128              ///< Base intensity level for the campfire effect (0-255, 8-bit PWM)
#define CAMPFIRE_MIN_INTENSITY 50                ///< Minimum intensity to prevent LED from turning off (0-255, 8-bit PWM)
#define CAMPFIRE_MAX_INTENSITY 255               ///< Maximum intensity for the campfire effect (0-255, 8-bit PWM)
#define CAMPFIRE_INTENSITY_CHANGE_PROBABILITY 50 ///< Probability of changing intensity per cycle (50% chance, 0-100)
#define CAMPFIRE_INTENSITY_STEP_MIN -10          ///< Minimum step for random intensity adjustments (-10 to 10)
#define CAMPFIRE_INTENSITY_STEP_MAX 11           ///< Maximum step for random intensity adjustments (-10 to 10, exclusive)
#define CAMPFIRE_PWM_PERIOD_US 40000             ///< Period for the software PWM cycle in microseconds (25 Hz)
#define CAMPFIRE_UPDATE_DELAY_MIN_MS 100         ///< Minimum delay between intensity updates in milliseconds
#define CAMPFIRE_UPDATE_DELAY_MAX_MS 500         ///< Maximum delay between intensity updates in milliseconds

// Gas lamp effect configuration constants
#define GASLAMP_PWM_PERIOD_US 10000                         ///< PWM period in microseconds (100Hz) for brightness control
#define GASLAMP_MAX_INTENSITY 250                           ///< Maximum brightness level (0-255, 8-bit PWM)
#define GASLAMP_TARGET_STABLE_INTENSITY 200                 ///< Target brightness in stable flame phase (0-255)
#define GASLAMP_STABLE_MIN_INTENSITY 160                    ///< Minimum brightness during stable phase (0-255)
#define GASLAMP_IGNITION_DURATION_MS 1000                   ///< Duration of the ignition phase in milliseconds
#define GASLAMP_INITIAL_FLICKER_DURATION_MS 500             ///< Duration of the initial flicker phase in milliseconds
#define GASLAMP_IGNITION_MIN_BRIGHTNESS 10                  ///< Minimum brightness during ignition phase (0-255)
#define GASLAMP_IGNITION_MAX_BRIGHTNESS 40                  ///< Maximum brightness during ignition phase (0-255)
#define GASLAMP_INITIAL_FLICKER_MIN_BRIGHTNESS 40           ///< Minimum brightness during initial flicker phase (0-255)
#define GASLAMP_INITIAL_FLICKER_MAX_BRIGHTNESS 90           ///< Maximum brightness during initial flicker phase (0-255)
#define GASLAMP_BRIGHTENING_STEP_MS 50                      ///< Interval for brightness increase steps during brightening phase in milliseconds
#define GASLAMP_BRIGHTENING_OFF_THRESHOLD 10                ///< Brightness threshold for transitioning to OFF state (0-255)
#define GASLAMP_STABLE_FLAME_INTERVAL_MS 1000               ///< Interval for flicker updates during stable flame phase in milliseconds
#define GASLAMP_STABLE_DELAY_MS 2000                        ///< Delay after stable flame or OFF state transitions in milliseconds
#define GASLAMP_IGNITION_FLICKER_MIN_DELAY_MS 300           ///< Minimum delay between flickers in ignition phase in milliseconds
#define GASLAMP_IGNITION_FLICKER_MAX_DELAY_MS 600           ///< Maximum delay between flickers in ignition phase in milliseconds
#define GASLAMP_INITIAL_FLICKER_MIN_DELAY_MS 30             ///< Minimum delay between flickers in initial flicker phase in milliseconds
#define GASLAMP_INITIAL_FLICKER_MAX_DELAY_MS 60             ///< Maximum delay between flickers in initial flicker phase in milliseconds
#define GASLAMP_INITIAL_FLICKER_TRANSITION_MIN_DELAY_MS 100 ///< Minimum delay before transitioning from initial flicker to brightening in milliseconds
#define GASLAMP_INITIAL_FLICKER_TRANSITION_MAX_DELAY_MS 300 ///< Maximum delay before transitioning from initial flicker to brightening in milliseconds
#define GASLAMP_STABLE_FLICKER_MIN_VARIATION -50            ///< Minimum brightness variation during stable flame phase (original)
#define GASLAMP_STABLE_FLICKER_MAX_VARIATION 60             ///< Maximum brightness variation during stable flame phase (original)
#define GASLAMP_STABLE_FLICKER_MIN_VARIATION_SUBTLE -20     ///< Minimum brightness variation during stable flame phase (subtle flicker)
#define GASLAMP_STABLE_FLICKER_MAX_VARIATION_SUBTLE 21      ///< Maximum brightness variation during stable flame phase (subtle flicker)
#define GASLAMP_STABLE_FLICKER_MIN_INTERVAL_MS 500          ///< Minimum interval for flicker updates in stable flame phase in milliseconds
#define GASLAMP_STABLE_FLICKER_MAX_INTERVAL_MS 1001         ///< Maximum interval for flicker updates in stable flame phase in milliseconds
#define GASLAMP_EXTINCTION_FLICKER_MIN_VARIATION -20        ///< Minimum brightness variation during extinction phase
#define GASLAMP_EXTINCTION_FLICKER_MAX_VARIATION 20         ///< Maximum brightness variation during extinction phase
#define GASLAMP_EXTINCTION_STEP_MS 100                      ///< Interval for brightness decrease steps during extinction phase in milliseconds
#define GASLAMP_EXTINCTION_DURATION_MS 1500                 ///< Duration of the extinction phase in milliseconds
#define GASLAMP_BRIGHTENING_RANGE_1_MAX 20                  ///< Upper brightness limit for first increment range in brightening phase (0-255)
#define GASLAMP_BRIGHTENING_RANGE_2_MAX 50                  ///< Upper brightness limit for second increment range in brightening phase (0-255)
#define GASLAMP_BRIGHTENING_INCREMENT_1 1                   ///< Brightness increment for first range (0 to GASLAMP_BRIGHTENING_RANGE_1_MAX)
#define GASLAMP_BRIGHTENING_INCREMENT_2 3                   ///< Brightness increment for second range (GASLAMP_BRIGHTENING_RANGE_1_MAX to GASLAMP_BRIGHTENING_RANGE_2_MAX)
#define GASLAMP_BRIGHTENING_INCREMENT_3 5                   ///< Brightness increment for third range (GASLAMP_BRIGHTENING_RANGE_2_MAX to GASLAMP_MAX_INTENSITY)

// Gas lamp defect effect configuration constants (issue #111) — reuses every
// GASLAMP_* constant above for ignition/flicker/brightening/extinction; these
// are only the added rare-malfunction-during-stable-flame parameters. Distinct
// namespace on purpose: must not touch GASLAMP_* (GasLamp) or DEFECT_LAMP_*
// (DefectLamp) — see doc/workshop/adding-a-device-type.md.
#define GASLAMPDEFECT_MALFUNCTION_CHANCE 300        ///< Chance per stable-flame tick of a glitch, out of GASLAMPDEFECT_MALFUNCTION_CHANCE_RANGE
#define GASLAMPDEFECT_MALFUNCTION_CHANCE_RANGE 1000 ///< Roll range for the malfunction chance (300/1000 = 30% per tick, ~1 glitch/2-3s at a ~750ms average flicker interval)
#define GASLAMPDEFECT_MALFUNCTION_MIN_MS 80       ///< Minimum malfunction duration in milliseconds
#define GASLAMPDEFECT_MALFUNCTION_MAX_MS 250      ///< Maximum malfunction duration in milliseconds
#define GASLAMPDEFECT_MALFUNCTION_MIN_BRIGHTNESS 0  ///< Minimum brightness during a malfunction (0-255)
#define GASLAMPDEFECT_MALFUNCTION_MAX_BRIGHTNESS 30 ///< Maximum brightness during a malfunction (0-255)

// Charlieplexing signal configuration constants
#define CHARLIEPLEXING_POV_MICROS 10000U             ///< Maximum time for one POV cycle in microseconds (100 Hz)
#define CHARLIEPLEXING_LIGHT_UP_CHANGE_TIME_MS 2000U ///< Duration of the lighting-up effect in milliseconds
#define CHARLIEPLEXING_TURN_OFF_CHANGE_TIME_MS 300U  ///< Duration of the turning-off effect in milliseconds
#define CHARLIEPLEXING_MS_TO_US 1000UL               ///< Conversion factor from milliseconds to microseconds

// Timing parameters (ms)
#define TRAFFIC_LIGHT_STABLE_DELAY_MS 2000UL     ///< Default duration for stable states (STOP, GO)
#define TRAFFIC_LIGHT_FLASH_ON_MS 40UL           ///< Amber flash ON duration
#define TRAFFIC_LIGHT_FLASH_OFF_MS 20UL          ///< Amber flash OFF duration
#define TRAFFIC_LIGHT_CAUTION_DURATION_MS 3000UL ///< Time yellow stays lit before STOP

// Servo configuration constants
#define LX16A_SERVO_ID 1                       ///< Default servo ID for bus communication (range: 0-253)
#define LX16A_BAUD_RATE 115200                 ///< Serial baud rate for LX-16A communication
#define SERVO_SPEED_MAX ((int16_t)1000)        ///< Maximum speed for LX-16A servo in motor mode (full forward)
#define SERVO_SPEED_MIN ((int16_t)-1000)       ///< Minimum speed for LX-16A servo in motor mode (full reverse)
#define SERVO_SPEED_STOP 0                     ///< Speed value to stop the servo in motor mode
#define SERVO_SPEED_DEFAULT ((SERVO_SPEED)300) ///< Default running speed applied at first start()

// Beacon effect configuration constants
#define BEACON_FLASH_ON_DURATION_1 80   ///< Duration of the first flash in milliseconds.
#define BEACON_FLASH_OFF_DURATION_1 100 ///< Pause duration after the first flash in milliseconds.
#define BEACON_FLASH_ON_DURATION_2 80   ///< Duration of the second flash in milliseconds.
#define BEACON_FLASH_OFF_DURATION_2 600 ///< Long pause duration after the second flash in milliseconds.
#define BEACON_SHORT_PAUSE 10           ///< Short pause for coroutine timing in milliseconds.

// Audio device constants
#define DFAUDIO_BAUD_RATE 8600 ///< Serial baud rate for audio device communication

// I²C bus pins — used by I2C_CARDS and I2C_SCAN. Override in config.h if needed.
#ifndef I2C_SDA
  #define I2C_SDA 21 ///< SDA pin for the shared I²C bus (ESP32 hardware default).
#endif
#ifndef I2C_SCL
  #define I2C_SCL 22 ///< SCL pin for the shared I²C bus (ESP32 hardware default).
#endif

// OLED pins — default to the shared I²C bus; override independently in config.h
// to put the OLED on a separate bus (e.g. address conflict or different wiring).
#ifndef OLED_SDA
  #define OLED_SDA I2C_SDA ///< OLED SDA — defaults to I2C_SDA (same bus as I2C_CARDS).
#endif
#ifndef OLED_SCL
  #define OLED_SCL I2C_SCL ///< OLED SCL — defaults to I2C_SCL (same bus as I2C_CARDS).
#endif

// OTA firmware update — used when OTA is defined in config.h.
#ifndef OTA_HOSTNAME
  #define OTA_HOSTNAME "layoutfx" ///< mDNS base name for ArduinoOTA; a MAC suffix is appended → layoutfx-xxxx.local
#endif
// OTA_PASSWORD — optional. If defined in config.h, espota and the web /update
// endpoint both require it. Strongly recommended on a shared/home network.
#ifndef OLED_HEIGHT
  #define OLED_HEIGHT 64 ///< Display height in pixels — 64 (SSD1306 0.96") or 32 (SSD1306 0.91").
#endif
#ifndef OLED_EVENT_MS
  #define OLED_EVENT_MS 3000 ///< Duration (ms) the event screen is shown before returning to idle.
#endif
