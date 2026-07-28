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
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-21
 * @license AGPL-3.0-or-later. See the LICENSE file in the project root for details.
 */

#pragma once

#include <LayoutFX_define.h>

#include "utils/ArduinoBoard.h"
#include <assert.h>
#ifdef LFX_DCC_ENABLED
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
#ifdef LFX_DCC_ENABLED
    dcc.pin(pin_id, pullup ? 1 : 0); // Enable Pullup
    dcc.init(MAN_ID_DIY, 3, 0, 0);   // Version 3, OpsModeAddressBaseCV=0
    _activePin = (int8_t)pin_id;
#endif
    forceLinkDccCallbacks();
    LOG_PRINT(F("[DCC] ready, pin "));
    LOG_PRINTLN(pin_id);

#ifdef LFX_DCC_AUDIT_ENABLED
    LOG_PRINTLN(F("[DCC] audit mode active"));
    resetSeenMessages();
#endif
  }

  /**
   * @brief Releases the DCC pin immediately (#19), so it can be reused by another
   *        device or reported as free, without waiting for a reboot.
   * @details NmraDcc has no teardown API — on ESP32, NmraDcc::pin()/init() attach
   *          the ISR via attachInterrupt(pin, ...) using the pin number we pass in,
   *          so detachInterrupt() on that same pin releases it cleanly from outside
   *          the library. Safe to call even if the DCC bus was never active.
   *          dcc.process() (called every loop() regardless) becomes a harmless no-op
   *          once detached: it only acts when its internal DataReady flag is set,
   *          which requires the now-detached ISR to have fired.
   */
  static void end() {
#ifdef LFX_DCC_ENABLED
    if (_activePin >= 0) {
      detachInterrupt(digitalPinToInterrupt((uint8_t)_activePin));
      _activePin = -1;
    }
#endif
  }

  /** @brief True while a DCC pin is currently attached (config declares a dcc bus). */
  static bool isActive() {
#ifdef LFX_DCC_ENABLED
    return _activePin >= 0;
#else
    return false;
#endif
  }

#ifdef LFX_DCC_AUDIT_ENABLED
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
#ifdef LFX_DCC_ENABLED
    dcc.process();
#endif
#ifdef LFX_DCC_AUDIT_ENABLED
    if (millis() - lastResetTime >= AUDIT_SAMPLING) {
      resetSeenMessages();
    }
#endif
  }

#ifdef LFX_DCC_ENABLED
  static void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower);

  static void notifyDccAccTurnoutBoard(uint16_t BoardAddr, uint8_t OutputPair, uint8_t Direction, uint8_t OutputPower);

  static void notifyDccState(uint16_t Addr, uint8_t State);

  static void notifyDccSigOutputState(uint16_t Addr, uint8_t State);

  static void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps);

  static void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState);

  static bool IS_OUTPUT_MODE() {
  #ifdef LFX_DCC_ENABLED
    return ((dcc.getCV(29) & CV29_OUTPUT_ADDRESS_MODE) != 0);
  #else
    return false;
  #endif
  }

  /**
   * @enum DccMsgKind
   * @brief Coarse DCC message categories tracked for the WebUI diagnostic screen.
   */
  enum DccMsgKind : uint8_t {
    DCC_MSG_RAW = 0,       ///< Any raw packet on the bus, decoded or not (proves the bus is alive).
    DCC_MSG_SPEED,         ///< Speed/direction packet.
    DCC_MSG_FUNC,          ///< Function group packet (F0-F12).
    DCC_MSG_ACCESSORY,     ///< Basic accessory packet (turnout).
    DCC_MSG_SIGNAL,        ///< Extended accessory packet (signal aspect).
    DCC_MSG_KIND_COUNT
  };

  /** @brief Record one received message of the given kind (called from the notify callbacks). */
  static void trackDccMsg(DccMsgKind kind) {
    if (kind >= DCC_MSG_KIND_COUNT)
      return;
    dccMsgCount[kind]++;
    dccMsgLastMs[kind] = millis();
  }

  /** @brief Packets seen since boot for this message kind. */
  static uint32_t dccMsgCountOf(DccMsgKind kind) { return (kind < DCC_MSG_KIND_COUNT) ? dccMsgCount[kind] : 0; }

  /** @brief millis() of the last packet of this kind, or 0 if never seen. */
  static unsigned long dccMsgLastMsOf(DccMsgKind kind) { return (kind < DCC_MSG_KIND_COUNT) ? dccMsgLastMs[kind] : 0; }

  /** @brief Per-category capacity of the decoded-event ring buffers exposed to the WebUI diagnostic log. */
  static constexpr uint8_t DCC_LOG_CAPACITY = 16;

  /**
   * @struct DccLogEntry
   * @brief One decoded DCC event, as shown on a single line of the WebUI diagnostic log.
   */
  struct DccLogEntry {
    DccMsgKind kind;
    uint16_t address;
    int16_t value;            ///< Mapped speed, function/accessory/signal state, in the same units passed to the device.
    unsigned long atMs;
    const __FlashStringHelper *deviceName;  ///< nullptr if no registered device matched this address.
    uint16_t repeatCount;      ///< 1 = seen once; >1 = this many consecutive identical packets collapsed into one line.
  };

  /**
   * @brief Append one decoded event to its category's ring buffer (overwrites that category's
   *        oldest entry once full).
   * @details Each message category (Speed/Func/Accessory/Signal) gets its own DCC_LOG_CAPACITY-slot
   *          buffer instead of sharing one pool — a command station repeats unchanged speed packets
   *          continuously while a throttle is held steady, and at full throttle the mapped speed
   *          jitters on nearly every repeat so it can't even be collapsed by the dedup below; with a
   *          single shared pool that traffic alone can cycle the whole buffer several times per
   *          second, evicting a rare Accessory/Signal burst before the WebUI's 1s poll ever sees it.
   *          Partitioning by category makes that structurally impossible. If this event is identical
   *          (kind/address/value/device) to the most recently logged one *in the same category*,
   *          bump its repeat counter in place instead of appending a new line.
   * @param deviceName Name of the device that reacted, or nullptr if the address matched nothing.
   */
  static void logDccEvent(DccMsgKind kind, uint16_t address, int16_t value, const __FlashStringHelper *deviceName) {
    if (kind >= DCC_MSG_KIND_COUNT)
      return;
    DccLogEntry *buf = dccLog[kind];
    uint8_t &head = dccLogHead[kind];
    uint8_t &count = dccLogCount[kind];
    if (count > 0) {
      DccLogEntry &last = buf[(head + DCC_LOG_CAPACITY - 1) % DCC_LOG_CAPACITY];
      if (last.address == address && last.value == value && last.deviceName == deviceName) {
        last.atMs = millis();
        if (last.repeatCount < 0xFFFF)
          last.repeatCount++;
        return;
      }
    }
    DccLogEntry &e = buf[head];
    e.kind = kind;
    e.address = address;
    e.value = value;
    e.atMs = millis();
    e.deviceName = deviceName;
    e.repeatCount = 1;
    head = (head + 1) % DCC_LOG_CAPACITY;
    if (count < DCC_LOG_CAPACITY)
      count++;
  }

  /** @brief Number of valid entries currently logged for `kind` (up to DCC_LOG_CAPACITY). */
  static uint8_t dccLogSize(DccMsgKind kind) { return (kind < DCC_MSG_KIND_COUNT) ? dccLogCount[kind] : 0; }

  /** @brief `kind`'s log entry at `index`, ordered oldest (0) to newest (dccLogSize(kind)-1). */
  static const DccLogEntry &dccLogAt(DccMsgKind kind, uint8_t index) {
    uint8_t start = (dccLogCount[kind] < DCC_LOG_CAPACITY) ? 0 : dccLogHead[kind];
    return dccLog[kind][(start + index) % DCC_LOG_CAPACITY];
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
   * @brief Sets a single loco function's on/off state for the device.
   * @param funcIndex Function number (0-28), decoded from a notifyDccFunc() FN_GROUP/FuncState pair.
   * @param on true if the function was just turned on, false if turned off.
   * @details Distinct from setDccFunction()/notifyDccState() (an unrelated, pre-existing
   *          accessory-address path): this hook is wired from notifyDccFunc(), the
   *          loco-address function-group path, and can identify *which* function changed —
   *          setDccFunction()'s single State byte cannot. No-op by default; devices that
   *          react to individual loco functions (e.g. DfRobotSerialMP3: next/prev/random on
   *          specific function numbers) override it.
   */
  virtual void setDccFunctionState(uint8_t funcIndex, bool on) {}

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
#ifdef LFX_DCC_ENABLED
  static NmraDcc dcc;

  // Pin currently attached to the NmraDcc ISR, or -1 when none (#19). Tracked here
  // rather than derived from DeviceFactory since this class owns the NmraDcc
  // interrupt lifecycle (attach in init(), detach in end()).
  static int8_t _activePin;

  // Per-category packet counters + last-seen timestamp, exposed via /api/dcc-status
  // for the WebUI diagnostic screen — always on (cheap: DCC_MSG_KIND_COUNT x 8 bytes),
  // independent of LFX_DCC_AUDIT_ENABLED which is a heavier Serial-only debug aid.
  static uint32_t dccMsgCount[DCC_MSG_KIND_COUNT];
  static unsigned long dccMsgLastMs[DCC_MSG_KIND_COUNT];

  // One ring buffer per message category for the WebUI diagnostic log — DCC_MSG_KIND_COUNT x
  // DCC_LOG_CAPACITY x ~14 bytes (~900 bytes total at DCC_LOG_CAPACITY=16), partitioned so
  // Speed/Func traffic can never evict an Accessory/Signal entry (#76). Still cheap enough for
  // the 2KB-RAM AVR/Nano targets that also compile this header when DCC_PIN is configured.
  static DccLogEntry dccLog[DCC_MSG_KIND_COUNT][DCC_LOG_CAPACITY];
  static uint8_t dccLogHead[DCC_MSG_KIND_COUNT];   ///< Index where the next entry will be written, per category.
  static uint8_t dccLogCount[DCC_MSG_KIND_COUNT];  ///< Number of valid entries per category (caps at DCC_LOG_CAPACITY).
#endif

#ifdef LFX_DCC_AUDIT_ENABLED

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

// #endif // LFX_DCC_ENABLED
