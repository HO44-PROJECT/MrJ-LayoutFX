/**
 * @file DccDrivableCallbacks.cpp
 * @brief Implementation of NmraDcc callback functions for controlling DCC devices.
 *
 * Defines callback functions for handling DCC commands (speed, signals, functions, and accessories)
 * in the MrJ-ArduinoRailwayFX project. Delegates speed commands to the DccDrivable class and provides
 * placeholders for handling signals, functions, and accessories (e.g., controlling LEDs).
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-21
 * @license MIT License
 */

#include "dcc/DccDrivable.h"

// Static variable initialization
uint8_t DccDrivable::DccDrivableDeviceNumber = 0;
#ifdef MRJFX_DCC_ENABLED
NmraDcc DccDrivable::dcc;
#endif
// Definitions of static members
DccDrivable *DccDrivable::DccDrivableDevices[MAX_PIN_NUMBER] = {nullptr};
ADDRESS DccDrivable::DccDrivableAddresses[MAX_PIN_NUMBER] = {0};

#ifdef MRJFX_DCC_AUDIT_ENABLED
uint8_t DccDrivable::dccSeenSpeed[DccDrivable::BITMAP_SIZE];
uint8_t DccDrivable::dccSeenFunc[DccDrivable::BITMAP_SIZE];
unsigned long DccDrivable::lastResetTime = 0;
#endif

#ifdef MRJFX_DCC_ENABLED
/**
 * @brief Handles DCC signal state commands for registered devices.
 * @param Addr DCC address of the device (1-10239).
 * @param State Signal state (e.g., 0 for off, non-zero for on).
 * @details Iterates through registered devices to find a matching DCC address and
 *          sets the function state for the corresponding device.
 */
void DccDrivable::notifyDccState(uint16_t Addr, uint8_t State) {
  #ifdef MRJFX_DCC_AUDIT_ENABLED
  DEBUG_PRINT(F("[notifyDccState] Addr="));
  DEBUG_PRINT(Addr);
  DEBUG_PRINT(F(", state="));
  DEBUG_PRINT(State);
  #endif

  // Iterate through registered devices to find matching address
  for (uint8_t i = 0; i < DccDrivableDeviceNumber; i++) {
    if (DccDrivableAddresses[i] == Addr) {
      // Set speed for the matching device
      DccDrivableDevices[i]->setDccFunction(State);
    }
  }
}

/**
 * @brief Handles DCC signal output state commands for registered devices.
 * @param Addr DCC address of the device (1-10239).
 * @param State Signal output state (e.g., 0 for off, non-zero for on).
 * @details Iterates through registered devices to find a matching DCC address and
 *          sets the signal output state for the corresponding device.
 */
void DccDrivable::notifyDccSigOutputState(uint16_t Addr, uint8_t State) {
  #ifdef MRJFX_DCC_AUDIT_ENABLED_AUDIT_ENABLED
  DEBUG_PRINT(F("[notifyDccSigOutputState] Addr="));
  DEBUG_PRINT(Addr);
  DEBUG_PRINT(F(", state="));
  DEBUG_PRINTLN(State);
  #endif
  for (uint8_t i = 0; i < DccDrivableDeviceNumber; i++) {
    if (DccDrivableAddresses[i] == Addr) {
      // Set speed for the matching device
      DccDrivableDevices[i]->setDccSigOutputState(State);
    }
  }
}

void DccDrivable::notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState) {
  #ifdef MRJFX_DCC_AUDIT_ENABLED_AUDIT_ENABLED

  if (Addr >= MAX_DCC_ADDR)
    return; // sécurité

  if (!GET_BIT(dccSeenFunc, Addr)) {
    SET_BIT(dccSeenFunc, Addr);

    DEBUG_PRINT(F("[notifyDccFunc] Addr="));
    DEBUG_PRINT(Addr);
    DEBUG_PRINT(F(", AddrType="));
    DEBUG_PRINT(AddrType);
    DEBUG_PRINT(F(", FuncGrp="));
    DEBUG_PRINT(FuncGrp);
    DEBUG_PRINT(F(", FuncState="));
    DEBUG_PRINTLN(FuncState);
  }

  #endif
}

/**
 * @brief Handles DCC speed commands for registered devices.
 * @param Addr DCC address of the device (1-10239).
 * @param AddrType Address type (DCC_ADDR_SHORT or DCC_ADDR_LONG).
 * @param Speed Speed value (0-127 for 128 steps, 0-29 for 28 steps, 0-15 for 14 steps).
 * @param Dir Direction (DCC_DIR_FWD or DCC_DIR_REV).
 * @param SpeedSteps Speed step mode (SPEED_STEP_14, SPEED_STEP_28, SPEED_STEP_128).
 * @details Iterates through registered devices to find a matching DCC address, maps the
 *          speed based on the speed step mode, and applies it to the corresponding device.
 *          Supports 14, 28, and 128 speed steps with direction handling.
 */
void DccDrivable::notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps) {
  #ifdef MRJFX_DCC_AUDIT_ENABLED

  if (Addr >= MAX_DCC_ADDR)
    return; // sécurité

  if (!GET_BIT(dccSeenSpeed, Addr)) {
    SET_BIT(dccSeenSpeed, Addr);

    DEBUG_PRINT(F("[notifyDccSpeed] Addr="));
    DEBUG_PRINT(Addr);
    DEBUG_PRINT(F(", Speed="));
    DEBUG_PRINT(Speed);
    DEBUG_PRINT(F(", Direction="));
    DEBUG_PRINT(Dir);
    DEBUG_PRINT(F(", SpeedSteps="));
    DEBUG_PRINTLN(SpeedSteps);
  }
  #endif

  // Iterate through registered devices to find matching address
  for (uint8_t i = 0; i < DccDrivableDeviceNumber; i++) {
    if (DccDrivableAddresses[i] == Addr) {
      int16_t mappedSpeed = 0;

      // Map speed based on SpeedSteps mode
      if (SpeedSteps == SPEED_STEP_128) {
        mappedSpeed = (Speed <= 1) ? 0 : map(Speed, 2, 127, 0, 1000);
        if (Dir == DCC_DIR_REV) {
          mappedSpeed = -mappedSpeed; // Reverse for backward direction
        }
      } else if (SpeedSteps == SPEED_STEP_28) {
        mappedSpeed = (Speed <= 1) ? 0 : map(Speed, 2, 29, 0, 1000);
        if (Dir == DCC_DIR_REV) {
          mappedSpeed = -mappedSpeed; // Reverse for backward direction
        }
      }
  #ifdef NMRA_DCC_ENABLE_14_SPEED_STEP_MODE
      else if (SpeedSteps == SPEED_STEP_14) {
        mappedSpeed = (Speed <= 1) ? 0 : map(Speed, 2, 15, 0, 1000);
        if (Dir == DCC_DIR_REV) {
          mappedSpeed = -mappedSpeed; // Reverse for backward direction
        }
      }
  #endif

      // Set speed for the matching device
      DccDrivableDevices[i]->setDccSpeed(mappedSpeed);
    }
  }
}

void DccDrivable::notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower) {
  #ifdef MRJFX_DCC_AUDIT_ENABLED
  DEBUG_PRINT(F("[notifyDccAccTurnoutOutput] Addr="));
  DEBUG_PRINT(Addr);
  DEBUG_PRINT(F(", direction="));
  DEBUG_PRINT(Direction);
  DEBUG_PRINT(F(", OutputPower ="));
  DEBUG_PRINTLN(OutputPower);
  #endif

  for (uint8_t i = 0; i < DccDrivableDeviceNumber; i++) {
    if (DccDrivableAddresses[i] == Addr) {
      DccDrivableDevices[i]->setDccAccessoryState(Direction);
    }
  }
}

/**
 * @brief Handles DCC accessory board commands for registered devices.
 * @param BoardAddr Board address of the accessory.
 * @param OutputPair Output pair number for the accessory.
 * @param Direction Direction of the accessory command (0 or 1).
 * @param OutputPower Output power state (0 for off, 1 for on).
 * @details Iterates through registered devices to find a matching board address and
 *          sets the accessory state for the corresponding device.
 */
void DccDrivable::notifyDccAccTurnoutBoard(uint16_t BoardAddr, uint8_t OutputPair, uint8_t Direction, uint8_t OutputPower) {
  #ifdef MRJFX_DCC_AUDIT_ENABLED
  DEBUG_PRINT(F("[notifyDccAccTurnoutBoard] Addr="));
  DEBUG_PRINT(BoardAddr);
  DEBUG_PRINT(F(", OutputPair="));
  DEBUG_PRINT(OutputPair);
  DEBUG_PRINT(F(", direction="));
  DEBUG_PRINT(Direction);
  DEBUG_PRINT(F(", OutputPower="));
  DEBUG_PRINTLN(OutputPower);
  #endif
  for (uint8_t i = 0; i < DccDrivableDeviceNumber; i++) {
    if (DccDrivableAddresses[i] == BoardAddr) {
      // Set state for the matching device
      DccDrivableDevices[i]->setDccAccessoryState(Direction);
    }
  }
}

#endif
