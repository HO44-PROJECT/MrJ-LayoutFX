/**
 * @file DccDrivable.h
 * @brief Abstract base class for DCC-controlled devices with cooperative multitasking.
 *
 * This header defines the `DccDrivable` class, which serves as an abstract base class for
 * managing hardware devices (e.g., LEDs, servos) in a model railway system. It integrates
 * with the NmraDcc library for DCC protocol handling and AceRoutine for coroutine-based
 * cooperative multitasking. Subclasses must implement pure virtual methods to define
 * device-specific behaviors. Pin initialization is explicitly triggered via the `initPins`
 * method to ensure proper setup.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-21
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#pragma once

#include <MrJRailwayFX_define.h>

// #ifdef MRJFX_DCC_ENABLED

#include "utils/ArduinoBoard.h"
#include <assert.h>
#ifdef MRJFX_DCC_ENABLED
  #include <NmraDcc.h>
#endif
#include "utils/utils.h"

// Type alias for DCC address
typedef uint16_t ADDRESS;

// Constant for invalid DCC device
#define NOT_A_DCC_DEVICE 255

extern void forceLinkDccCallbacks();

/**
 * @class DccDrivable
 * @brief Abstract base class for DCC-controlled devices with coroutine support.
 *
 * Provides a framework for managing devices (e.g., LEDs, servos) using a state machine
 * driven by coroutines. Supports ON/OFF states and DCC speed commands. Subclasses must
 * override pure virtual methods to implement specific device behaviors.
 */
class DccDrivable {
public:
  /**
   * @brief Default constructor for DccDrivable.
   * @details Initializes the NmraDcc base class for DCC protocol handling.
   */
  DccDrivable() {}

  /**
   * @brief Initializes the DCC pin and NmraDcc library.
   * @param pin_id The Arduino pin ID for DCC signal input.
   * @details Configures the specified pin with a pull-up resistor and initializes
   *          the NmraDcc library with DIY manufacturer ID, version 3, and default CVs.
   */
  static void init(uint8_t pin_id, bool pullup = true) {
#ifdef MRJFX_DCC_ENABLED
    dcc.pin(pin_id, pullup ? 1 : 0); // Enable Pullup
    dcc.init(MAN_ID_DIY, 3, 0, 0);   // Version 3, OpsModeAddressBaseCV=0
#endif
    forceLinkDccCallbacks();
    DEBUG_PRINT(F("DCC ready, pin "));
    DEBUG_PRINTLN(pin_id);

#ifdef MRJFX_DCC_AUDIT_ENABLED
    DEBUG_PRINTLN(F("DCC Audit mode Activated"));
    resetSeenMessages();
#endif
  }

#ifdef MRJFX_DCC_AUDIT_ENABLED
  static void resetSeenMessages() {
    memset(dccSeenSpeed, 0, BITMAP_SIZE);
    memset(dccSeenFunc, 0, BITMAP_SIZE);
    lastResetTime = millis();
  }
#endif

  /**
   * @brief Processes DCC commands in the main loop.
   * @details Calls the NmraDcc process function to handle incoming DCC packets.
   */
  static void loop() {
#ifdef MRJFX_DCC_ENABLED
    dcc.process();
#endif
#ifdef MRJFX_DCC_AUDIT_ENABLED
    if (millis() - lastResetTime >= AUDIT_SAMPLING) {
      resetSeenMessages();
    }
#endif
  }

#ifdef MRJFX_DCC_ENABLED
  static void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower);

  static void notifyDccAccTurnoutBoard(uint16_t BoardAddr, uint8_t OutputPair, uint8_t Direction, uint8_t OutputPower);

  static void notifyDccState(uint16_t Addr, uint8_t State);

  static void notifyDccSigOutputState(uint16_t Addr, uint8_t State);

  static void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps);

  static void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState);

  static bool IS_OUTPUT_MODE() {
  #ifdef MRJFX_DCC_ENABLED
    return ((dcc.getCV(29) & CV29_OUTPUT_ADDRESS_MODE) != 0);
  #else
    return false;
  #endif
  }
#endif

  /**
   * @brief Sets the DCC speed for the device.
   * @param Speed Mapped speed value (typically -1000 to 1000).
   * @details Pure virtual method to be overridden by subclasses to handle DCC speed
   *          commands specific to the device (e.g., setting motor speed or LED intensity).
   */
  virtual void setDccSpeed(int16_t Speed) {}

  /**
   * @brief Sets the DCC function state for the device.
   * @param State Function state bitmask (1 for on, 0 for off).
   * @details Pure virtual method to be overridden by subclasses to handle DCC function
   *          commands (e.g., toggling LEDs or enabling sounds).
   */
  virtual void setDccFunction(uint8_t State) {}

  /**
   * @brief Sets the DCC signal output state for the device.
   * @param State Signal output state (e.g., 0 for off, non-zero for on).
   * @details Pure virtual method to be overridden by subclasses to handle DCC signal
   *          output state commands.
   */
  virtual void setDccSigOutputState(uint8_t State) {}

  /**
   * @brief Sets the DCC accessory state for the device.
   * @param State Accessory state (0 for off, non-zero for on).
   * @details Pure virtual method to be overridden by subclasses to handle DCC accessory
   *          commands (e.g., turning an accessory on or off).
   */
  virtual void setDccAccessoryState(uint8_t State) {}

  /**
   * @brief Registers a device with its DCC address.
   * @param address DCC address of the device (1-10239).
   * @return uint8_t Device index or NOT_A_DCC_DEVICE if registration fails.
   * @details Adds the device and its DCC address to the static arrays for tracking.
   *          Returns the device index or NOT_A_DCC_DEVICE if the maximum number of
   *          devices is reached.
   */
  uint8_t registerDccDrivableDevice(ADDRESS address) {
    decoderAddress = address;
    if (DccDrivableDeviceNumber < MAX_PIN_NUMBER) {
      DccDrivableDevices[DccDrivableDeviceNumber] = this;
      DccDrivableAddresses[DccDrivableDeviceNumber] = address;
      DccDrivableDeviceNumber++;
      return DccDrivableDeviceNumber;
    }
    return NOT_A_DCC_DEVICE;
  }

  /** @brief Returns the registered DCC address, or 0 if not registered. */
  ADDRESS getDccAddress() const { return decoderAddress; }

protected:
  // DCC address for the device
  ADDRESS decoderAddress = 0;
#ifdef MRJFX_DCC_ENABLED
  static NmraDcc dcc;
#endif

#ifdef MRJFX_DCC_AUDIT_ENABLED

  static constexpr uint16_t AUDIT_SAMPLING = 1000;
  static constexpr uint16_t MAX_DCC_ADDR = 10240;
  static constexpr uint16_t BITMAP_SIZE = MAX_DCC_ADDR / 8;

  static uint8_t dccSeenSpeed[BITMAP_SIZE];
  static uint8_t dccSeenFunc[BITMAP_SIZE];
  static unsigned long lastResetTime;

  // --- Macros de manipulation du bitmap ---
  static inline void SET_BIT(uint8_t *bitmap, uint16_t addr) { bitmap[addr / 8] |= (1 << (addr % 8)); }
  static inline bool GET_BIT(uint8_t *bitmap, uint16_t addr) { return bitmap[addr / 8] & (1 << (addr % 8)); }
#endif

private:
  // Array to store registered devices
  static DccDrivable *DccDrivableDevices[MAX_PIN_NUMBER];
  // Array to store DCC addresses of devices
  static ADDRESS DccDrivableAddresses[MAX_PIN_NUMBER];
  // Number of registered devices
  static uint8_t DccDrivableDeviceNumber;
};

// #endif // MRJFX_DCC_ENABLED
