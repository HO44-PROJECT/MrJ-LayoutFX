/**
 * @file Device.h
 * @brief Defines the `Device` abstract base class for asynchronous device control.
 *
 * This header file provides a minimal interface for managing hardware devices using
 * cooperative multitasking via the AceRoutine library. It supports a basic ON_STATE/OFF_STATE
 * model and is designed for extension by specialized subclasses (e.g., traffic signals,
 * relays, lamps). Derived classes must override pure virtual methods to implement specific
 * behaviors. Pin initialization must be triggered explicitly by calling `initPins` when needed.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include <MrJRailwayFX_define.h>

#include "devices/PinState.h"

#ifdef MRJFX_SPI_CARDS_ENABLED
  #include "spi/Spi595Bus.h"
#endif

#include "dcc/DccDrivable.h"
#include "devices/Pov.h"
#include "utils/ArduinoBoard.h"
#include "utils/utils.h"
#include <AceRoutine.h>
#include <assert.h>

using namespace ace_routine;

/**
 * @typedef STATE_TYPE
 * @brief Type alias for the device's state representation.
 *
 * Defines the type used for representing the operational state of a device (e.g., ON_STATE, OFF_STATE).
 */
typedef int8_t STATE_TYPE;

struct TransitionPtr {
  const STATE_TYPE *ptr;
  STATE_TYPE operator[](size_t idx) const {
    return pgm_read_byte(ptr + idx);
  }
  explicit operator bool() const {
    return ptr != nullptr;
  }
  // accès direct à l’élément courant (*p)
  STATE_TYPE operator*() const {
    return pgm_read_byte(ptr);
  }
  TransitionPtr &operator++() {
    ++ptr;
    return *this;
  }
};

#define DEVICE_WAIT_STATE_CHANGE(target) COROUTINE_AWAIT((this->getState() != (target)))

/**
 * @class Device
 * @brief Abstract base class for asynchronous device control using coroutines.
 *
 * This class provides a foundation for managing hardware devices with a state machine
 * driven by coroutines. It supports basic ON_STATE/OFF_STATE transitions and pin management.
 * Subclasses must implement pure virtual methods (`runCoroutine`, `getPinCount`, `setPin`,
 * `getPin`) to define specific behaviors. Pin initialization is performed explicitly by
 * calling `initPins` when required by the caller.
 */
class Device : public Coroutine, public DccDrivable {
public:
  /**
   * @brief Constant for the device's off state.
   */
  static const STATE_TYPE OFF_STATE = 0;

  static const STATE_TYPE NEXT_STABLE = OFF_STATE + 1;

  /**
   * @brief Constant for the device's stable but active state.
   */
  static const STATE_TYPE RUN_STABLE_STATE = 101;

  /**
   * @brief Constant for the device's ininterruptible default in-transit state.
   */
  static const STATE_TYPE RUN_TRANSIT_STATE = -100;

  /**
   * @brief Constant for the device's stable but just after a target state change.
   */
  static const STATE_TYPE INIT_STATE = -101;

  static const STATE_TYPE NEXT_NON_STABLE = -1;

  // Déclaration d'une fonction virtuelle pure
  virtual const __FlashStringHelper *getDeviceName() const {
    return F("Unknown");
  }

  /**
   * @brief Returns the number of distinct named states (including OFF).
   *
   * Used by the WebUI to decide whether to show a toggle (2 states) or
   * labelled state buttons (> 2 states). Override in multi-state devices.
   *
   * @return uint8_t Number of valid states, counting from 0 (OFF).
   */
  virtual uint8_t getStateCount() const { return 2; }

  /**
   * @brief Constructs a `Device` instance.
   *
   * Initializes the device with default states (OFF_STATE). Subclasses should extend
   * this constructor to configure specific properties.
   */
  Device() = default;

  /**
   * @brief Sets the desired state for the device from the DCC or the user point of view.
   *
   * Updates the desired state, which will be processed once in a stable state.
   *
   * @param newState The new desired state (e.g., ON_STATE or OFF_STATE).
   */
  virtual void newState(STATE_TYPE newState) {
    desiredState = newState;

    activateNewTarget();
  }

  /**
   * @brief Requests the device to switch to the OFF_STATE.
   *
   * Calls `trigger(OFF_STATE)` to deactivate the device asynchronously.
   */
  inline virtual void switchOff() {
    // static const char MSG_SPEED_OFF[] PROGMEM = "%S: switch off"; // Déclarer en PROGMEM
    // DEBUG_PRINTLN(MSG_SPEED_OFF, getDeviceName());

    newState(OFF_STATE);
  }

  /**
   * @brief Checks if the device is in a non-interruptible state transition.
   *
   * Returns `true` if the device is in a non-interruptible transition state (RUN_TRANSIT_STATE or internal working state).
   *
   * @return bool `true` if the device is transitioning, `false` if the state is settled or active but interruptible.
   */
  inline bool busy() {
    return state < 0;
  }

  /**
   * @brief Gets the current state of the device.
   *
   * @return STATE_TYPE The current operational state.
   */
  inline STATE_TYPE getState() const {
    return state;
  }

  /**
   * @brief Gets the desired state of the device for the next transition.
   *
   * @return STATE_TYPE The desired state for the next transition.
   */
  inline STATE_TYPE getDesiredState() const {
    return desiredState;
  }

  /**
   * @brief Gets the current target state of the device for the current transition.
   *
   * @return STATE_TYPE The target state for the current transition.
   */
  inline STATE_TYPE getTargetState() const {
    return targetState;
  }

  /**
   * @brief Executes the coroutine for asynchronous state transitions.
   *
   * Pure virtual function that must be implemented by subclasses to define specific
   * state transition behaviors using the AceRoutine library.
   *
   * @return int The coroutine state as defined by AceRoutine.
   */
  virtual int runCoroutine() = 0;

  /**
   * @brief Returns the number of output pins managed by the device.
   *
   * Pure virtual function that must be implemented by subclasses to specify the number
   * of pins used by the device.
   *
   * @return size_t The number of output pins.
   */
  virtual size_t getPinCount() const = 0;

  /**
   * @brief Sets the output pins for the device from an array.
   *
   * Updates the pin configuration using the provided array. The caller is responsible
   * for calling `initPins` explicitly to apply the pin configuration to the hardware.
   *
   * @param pin_count The number of valid pins in the array.
   * @param pins The array of pin identifiers.
   */
  virtual bool setPins(size_t pin_count, const PIN_ID pins[]) {
    // DEBUG_PRINTLN("set_pin");
    // DEBUG_PRINTLN(getPinCount());
    // Assign pins from the input array, filling unused slots with NO_PIN
    for (size_t i = 0; i < getPinCount(); i++) {
      setPin(i, i < pin_count ? pins[i] : NO_PIN);
    }
    if (pin_count > 0 && label == ' ') {
      setLabel(toHexChar(pinId(pins[0])));
    }
    // TODO
    // return validatePins();
    return true;
  }

#ifndef MRJFX_SPI_CARDS_ENABLED
  virtual bool setPins(size_t pin_count, ...) {
    va_list args;
    va_start(args, pin_count);

    for (size_t i = 0; i < getPinCount(); i++) {
      PIN_ID pin = (i < pin_count) ? (PIN_ID)va_arg(args, int) : NO_PIN;
      setPin(i, pin);
    }

    va_end(args);

    if (pin_count > 0 && label == ' ') {
      setLabel(toHexChar(getPin(0)));
    }

    return true;
  }
#endif // !MRJFX_SPI_CARDS_ENABLED
  virtual bool validatePins() {
    // DEBUG_PRINTF("validate pins for device %s\n", getDeviceName());
    // DEBUG_PRINT(F("Validate pins for device "));
    // DEBUG_PRINTLN(getDeviceName());

    validePins = true;
    for (size_t i = 0; i < getPinCount(); i++) {
      if (!VALID_PIN(getPin(i))) {
        validePins = false;
        // DEBUG_PRINT("Pin #");
        // DEBUG_PRINT(i);
        // DEBUG_PRINT("(");
        // DEBUG_PRINT(getPin(i));
        // DEBUG_PRINT(")");
        // DEBUG_PRINTLN(" is invalid.");
        break;
      }
      // DEBUG_PRINT("Pin #");
      // DEBUG_PRINT(i);
      // DEBUG_PRINT("(");
      // DEBUG_PRINT(getPin(i));
      // DEBUG_PRINT(")");
      // DEBUG_PRINTLN(" is valid.");
    }

    // DEBUG_PRINTLN(validePins ? "ok" : "KO");

    return validePins;
  }

  /**
   * @brief Retourne le résultat de la dernière validation des broches.
   * @return L'état de validation des broches.
   */
  bool arePinsValid() const {
    return validePins;
  }

  /**
   * @brief Assigns hardware output pins using variadic arguments.
   *
   * Updates the pin configuration using a variable number of pin identifiers. The caller
   * is responsible for calling `initPins` explicitly to apply the pin configuration.
   *
   * @param pin_count The number of pins provided.
   * @param ... A variable list of PIN_ID arguments.
   */
#ifndef MRJFX_SPI_CARDS_ENABLED
  virtual bool setVarPins(size_t pin_count, ...) {
    {
      va_list args;
      va_start(args, pin_count);

      for (size_t i = 0; i < getPinCount(); i++) {
        setPin(i, i < pin_count ? static_cast<PIN_ID>(va_arg(args, int)) : NO_PIN);
      }

      va_end(args);

      return validatePins();
    }
  }
#endif // !MRJFX_SPI_CARDS_ENABLED

  /**
   * @brief Initializes the device's pins.
   *
   * Configures all valid pins (not NO_PIN) with their default inactive state.
   * This method must be called explicitly by the caller when pin initialization is needed.
   */
  virtual bool initPins() {
    if (validatePins()) {
      for (size_t i = 0; i < getPinCount(); ++i) {
        PIN_ID pin = getPin(i);
        if (pin != NO_PIN) {
          pin_it(pin, inactive_state);
        }
      }
      return true;
    }
    return false;
  }

  virtual bool handlePinInitFailure() {
    // Initialize the output pin.
    if (!initPins()) {
      state = targetState = OFF_STATE;
      COROUTINE_YIELD();
      return false;
    }
    return true;
  }

  /**
   * @brief Sets the output pin for the device at the specified index.
   *
   * Pure virtual function that must be implemented by subclasses to assign a pin
   * identifier at the given index.
   *
   * @param index The index of the pin to set (0 to getPinCount() - 1).
   * @param pin The pin identifier to assign.
   */
  virtual bool setPin(size_t index, PIN_ID pin) = 0;

  /**
   * @brief Gets the pin identifier at the specified index.
   *
   * Pure virtual function that must be implemented by subclasses to retrieve the
   * pin identifier at the given index.
   *
   * @param index The index of the pin to retrieve (0 to getPinCount() - 1).
   * @return PIN_ID The pin identifier, or NO_PIN if invalid.
   */
  virtual PIN_ID getPin(size_t index) const = 0;

  /**
   * @brief Activates the specified pin (GPIO or SPI daughter card).
   */
  void outputActive(PIN_ID pin) {
#ifdef MRJFX_SPI_CARDS_ENABLED
    if (pin.isSpi()) {
      Spi595Bus::setPin(pin.card, pin.pin, active_state.value);
      return;
    }
    digitalWrite(pin.pin, active_state.value);
#else
    digitalWrite(pin, active_state.value);
#endif
  }

  /**
   * @brief Deactivates the specified pin (GPIO or SPI daughter card).
   */
  void outputInactive(PIN_ID pin) {
#ifdef MRJFX_SPI_CARDS_ENABLED
    if (pin.isSpi()) {
      Spi595Bus::setPin(pin.card, pin.pin, inactive_state.value);
      return;
    }
    digitalWrite(pin.pin, inactive_state.value);
#else
    digitalWrite(pin, inactive_state.value);
#endif
  }

  /**
   * @brief Virtual destructor for proper cleanup in derived classes.
   */
  virtual ~Device() = default;

  virtual inline void setLabel(char newLabel) {
    label = newLabel;
    for (uint8_t i = 0; i < getPinCount(); i++) {
      STATUS_LABEL(pinId(getPin(i)), label);
    }
  }

  inline virtual char statusChar() {
    switch (state) {
    case OFF_STATE:
      return 'X';
    case RUN_STABLE_STATE:
      return 'O';
    case RUN_TRANSIT_STATE:
      return 't';
    case INIT_STATE:
      return 'i';
    }
    DEBUG_PRINT(F("Unknown state char "));
    DEBUG_PRINTLN(state);
    return '?';
  }

protected:
  virtual bool activateNewTarget() {
    // DEBUG_PRINTLN(F("activateNewTarget device"));
    if (!busy()) {
      DEBUG_PRINTLN(F("activateNewTarget not busy"));
      DEBUG_PRINTLN(targetState);
      DEBUG_PRINTLN(desiredState);
      if (targetState != desiredState) {
        // DEBUG_PRINTLN("%s: New state %d", getDeviceName(), desiredState);
        targetState = desiredState;
        state = INIT_STATE;
        return true;
      }
    }
    return false;
  }

  /*
      device has changed its state. If the new state is stable, take care of potential new desiredState
  */
  virtual void setState(STATE_TYPE newState) {
    DEBUG_PRINT(F("Set state as "));
    DEBUG_PRINTLN(newState);

    state = newState;

    // Refresh status on display
    for (uint8_t i = 0; i < getPinCount(); i++) {
      STATUS(pinId(getPin(i)), statusChar());
    }

    activateNewTarget();
  }

  /**
   * @brief Configures the mode and value of a pin.
   *
   * Sets the specified pin’s mode and value based on the provided state. Ignores
   * invalid pins (NO_PIN).
   *
   * @param pin The pin number to configure.
   * @param state A PIN_STATE object defining the mode and value.
   */
  virtual void pin_it(PIN_ID pin, PIN_STATE state) {
    if (pin == NO_PIN)
      return;
#ifdef MRJFX_SPI_CARDS_ENABLED
    if (pin.isSpi()) {
      Spi595Bus::setPin(pin.card, pin.pin, state.value);
      return;
    }
    pinMode(pin.pin, state.mode);
    digitalWrite(pin.pin, state.value);
#else
    pinMode(pin, state.mode);
    digitalWrite(pin, state.value);
#endif
  }

  PIN_STATE active_state = H;   ///< Active output state (default: high).
  PIN_STATE inactive_state = L; ///< Inactive output state (default: low).

  bool validePins = false;

protected:
  STATE_TYPE state = OFF_STATE;        ///< Current operational state of the device. use setState or getState.
  STATE_TYPE targetState = OFF_STATE;  ///< Target state for the current transition. (stable). Use getTargetState. target is modified by activateNewTarget
  STATE_TYPE desiredState = OFF_STATE; ///< Target state for the next transition. (stable). use newState or getDesiredState

  char label = ' ';

  char toHexChar(int value) {
    if (value >= 0 && value <= 9) {
      return '0' + value; // '0' -> '9'
    } else if (value >= 10 && value < 16) {
      return 'A' + (value - 10); // 'A' -> 'F'
    } else {
      return '?'; // valeur invalide
    }
  }
};
