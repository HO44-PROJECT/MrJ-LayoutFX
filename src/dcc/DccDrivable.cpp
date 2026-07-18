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
#include "devices/Device.h"

#ifdef LFX_OLED_ENABLED
  #include "config/ConfigManager.h"
  #include "oled/OledDisplay.h"
#endif

// Static variable initialization
uint8_t DccDrivable::DccDrivableDeviceNumber = 0;
#ifdef LFX_DCC_ENABLED
NmraDcc DccDrivable::dcc;
int8_t DccDrivable::_activePin = -1;
uint32_t DccDrivable::dccMsgCount[DccDrivable::DCC_MSG_KIND_COUNT] = {0};
unsigned long DccDrivable::dccMsgLastMs[DccDrivable::DCC_MSG_KIND_COUNT] = {0};
DccDrivable::DccLogEntry DccDrivable::dccLog[DccDrivable::DCC_MSG_KIND_COUNT][DccDrivable::DCC_LOG_CAPACITY] = {};
uint8_t DccDrivable::dccLogHead[DccDrivable::DCC_MSG_KIND_COUNT] = {0};
uint8_t DccDrivable::dccLogCount[DccDrivable::DCC_MSG_KIND_COUNT] = {0};
#endif
// Definitions of static members
DccDrivable *DccDrivable::DccDrivableDevices[MAX_PIN_NUMBER] = {nullptr};
ADDRESS DccDrivable::DccDrivableAddresses[MAX_PIN_NUMBER] = {0};

#if defined(LFX_DCC_ENABLED) && defined(LFX_OLED_ENABLED)
// Last state notified to the OLED per registered device slot (parallel to
// DccDrivableAddresses[]/DccDrivableDevices[]) — DCC command stations repeat
// packets continuously, so without this the event screen would flicker on
// every repeat of an unchanged state/speed instead of only on real transitions (#46).
static int16_t _dccLastNotified[MAX_PIN_NUMBER];
static bool _dccEverNotified[MAX_PIN_NUMBER] = {false};

// Post an OLED event for slot i only if `value` differs from what was last
// notified for that slot (or this is the first notify for it).
static void _dccNotifyOledIfChanged(uint8_t i, Device *dev, int value) {
  if (_dccEverNotified[i] && _dccLastNotified[i] == value)
    return;
  _dccLastNotified[i] = value;
  _dccEverNotified[i] = true;
  const char *id = ConfigManager::factory().idOf(dev);
  OledDisplay::notify(String(dev->getDeviceName()).c_str(), id, value);
}
#endif

#ifdef LFX_DCC_AUDIT_ENABLED
uint8_t DccDrivable::dccSeenSpeed[DccDrivable::BITMAP_SIZE];
uint8_t DccDrivable::dccSeenFunc[DccDrivable::BITMAP_SIZE];
unsigned long DccDrivable::lastResetTime = 0;
#endif

#ifdef LFX_DCC_ENABLED
/**
 * @brief Handles DCC signal state commands for registered devices.
 * @param Addr DCC address of the device (1-10239).
 * @param State Signal state (e.g., 0 for off, non-zero for on).
 * @details Iterates through registered devices to find a matching DCC address and
 *          sets the function state for the corresponding device.
 */
void DccDrivable::notifyDccState(uint16_t Addr, uint8_t State) {
  #ifdef LFX_DCC_AUDIT_ENABLED
  MRJ_DEBUG_PRINT(F("[notifyDccState] Addr="));
  MRJ_DEBUG_PRINT(Addr);
  MRJ_DEBUG_PRINT(F(", state="));
  MRJ_DEBUG_PRINT(State);
  #endif

  // Iterate through registered devices to find matching address
  for (uint8_t i = 0; i < DccDrivableDeviceNumber; i++) {
    if (DccDrivableAddresses[i] == Addr) {
      // Set speed for the matching device
      DccDrivableDevices[i]->setDccFunction(State);
      #ifdef LFX_OLED_ENABLED
      _dccNotifyOledIfChanged(i, static_cast<Device *>(DccDrivableDevices[i]), State & 0x01);
      #endif
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
  trackDccMsg(DCC_MSG_SIGNAL);
  #ifdef LFX_DCC_AUDIT_ENABLED
  MRJ_DEBUG_PRINT(F("[notifyDccSigOutputState] Addr="));
  MRJ_DEBUG_PRINT(Addr);
  MRJ_DEBUG_PRINT(F(", state="));
  MRJ_DEBUG_PRINTLN(State);
  #endif
  bool matched = false;
  for (uint8_t i = 0; i < DccDrivableDeviceNumber; i++) {
    if (DccDrivableAddresses[i] == Addr) {
      matched = true;
      // Set speed for the matching device
      DccDrivableDevices[i]->setDccSigOutputState(State);
      logDccEvent(DCC_MSG_SIGNAL, Addr, State, static_cast<Device *>(DccDrivableDevices[i])->getDeviceName());
      #ifdef LFX_OLED_ENABLED
      _dccNotifyOledIfChanged(i, static_cast<Device *>(DccDrivableDevices[i]), State);
      #endif
    }
  }
  if (!matched)
    logDccEvent(DCC_MSG_SIGNAL, Addr, State, nullptr);
}

void DccDrivable::notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState) {
  trackDccMsg(DCC_MSG_FUNC);
  #ifdef LFX_DCC_AUDIT_ENABLED

  if (Addr >= MAX_DCC_ADDR)
    return; // sécurité

  if (!GET_BIT(dccSeenFunc, Addr)) {
    SET_BIT(dccSeenFunc, Addr);

    MRJ_DEBUG_PRINT(F("[notifyDccFunc] Addr="));
    MRJ_DEBUG_PRINT(Addr);
    MRJ_DEBUG_PRINT(F(", AddrType="));
    MRJ_DEBUG_PRINT(AddrType);
    MRJ_DEBUG_PRINT(F(", FuncGrp="));
    MRJ_DEBUG_PRINT(FuncGrp);
    MRJ_DEBUG_PRINT(F(", FuncState="));
    MRJ_DEBUG_PRINTLN(FuncState);
  }

  #endif

  // No registered device currently reacts to raw function-group packets (setDccFunction
  // is wired from notifyDccState, a distinct legacy path) — logged with no device name so
  // the diagnostic log still shows the packet arrived, which is the point of this callback.
  logDccEvent(DCC_MSG_FUNC, Addr, FuncState, nullptr);
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
  trackDccMsg(DCC_MSG_SPEED);
  #ifdef LFX_DCC_AUDIT_ENABLED

  if (Addr >= MAX_DCC_ADDR)
    return; // sécurité

  if (!GET_BIT(dccSeenSpeed, Addr)) {
    SET_BIT(dccSeenSpeed, Addr);

    MRJ_DEBUG_PRINT(F("[notifyDccSpeed] Addr="));
    MRJ_DEBUG_PRINT(Addr);
    MRJ_DEBUG_PRINT(F(", Speed="));
    MRJ_DEBUG_PRINT(Speed);
    MRJ_DEBUG_PRINT(F(", Direction="));
    MRJ_DEBUG_PRINT(Dir);
    MRJ_DEBUG_PRINT(F(", SpeedSteps="));
    MRJ_DEBUG_PRINTLN(SpeedSteps);
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
      logDccEvent(DCC_MSG_SPEED, Addr, mappedSpeed, static_cast<Device *>(DccDrivableDevices[i])->getDeviceName());
      #ifdef LFX_OLED_ENABLED
      // mappedSpeed changes on nearly every repeated packet at full throttle (throttle
      // jitter of +/-1 step), so dedup on it directly — same guard as the other two
      // callbacks, keeping the event screen quiet while the train holds a steady speed.
      _dccNotifyOledIfChanged(i, static_cast<Device *>(DccDrivableDevices[i]), mappedSpeed);
      #endif
    }
  }
}

void DccDrivable::notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower) {
  trackDccMsg(DCC_MSG_ACCESSORY);
  #ifdef LFX_DCC_AUDIT_ENABLED
  MRJ_DEBUG_PRINT(F("[notifyDccAccTurnoutOutput] Addr="));
  MRJ_DEBUG_PRINT(Addr);
  MRJ_DEBUG_PRINT(F(", direction="));
  MRJ_DEBUG_PRINT(Direction);
  MRJ_DEBUG_PRINT(F(", OutputPower ="));
  MRJ_DEBUG_PRINTLN(OutputPower);
  #endif

  bool matched = false;
  for (uint8_t i = 0; i < DccDrivableDeviceNumber; i++) {
    if (DccDrivableAddresses[i] == Addr) {
      matched = true;
      DccDrivableDevices[i]->setDccAccessoryState(Direction);
      logDccEvent(DCC_MSG_ACCESSORY, Addr, Direction, static_cast<Device *>(DccDrivableDevices[i])->getDeviceName());
      #ifdef LFX_OLED_ENABLED
      _dccNotifyOledIfChanged(i, static_cast<Device *>(DccDrivableDevices[i]), Direction);
      #endif
    }
  }
  if (!matched)
    logDccEvent(DCC_MSG_ACCESSORY, Addr, Direction, nullptr);
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
  trackDccMsg(DCC_MSG_ACCESSORY);
  #ifdef LFX_DCC_AUDIT_ENABLED
  MRJ_DEBUG_PRINT(F("[notifyDccAccTurnoutBoard] Addr="));
  MRJ_DEBUG_PRINT(BoardAddr);
  MRJ_DEBUG_PRINT(F(", OutputPair="));
  MRJ_DEBUG_PRINT(OutputPair);
  MRJ_DEBUG_PRINT(F(", direction="));
  MRJ_DEBUG_PRINT(Direction);
  MRJ_DEBUG_PRINT(F(", OutputPower="));
  MRJ_DEBUG_PRINTLN(OutputPower);
  #endif
  bool matched = false;
  for (uint8_t i = 0; i < DccDrivableDeviceNumber; i++) {
    if (DccDrivableAddresses[i] == BoardAddr) {
      matched = true;
      // Set state for the matching device
      DccDrivableDevices[i]->setDccAccessoryState(Direction);
      logDccEvent(DCC_MSG_ACCESSORY, BoardAddr, Direction, static_cast<Device *>(DccDrivableDevices[i])->getDeviceName());
      #ifdef LFX_OLED_ENABLED
      _dccNotifyOledIfChanged(i, static_cast<Device *>(DccDrivableDevices[i]), Direction);
      #endif
    }
  }
  if (!matched)
    logDccEvent(DCC_MSG_ACCESSORY, BoardAddr, Direction, nullptr);
}

#endif
