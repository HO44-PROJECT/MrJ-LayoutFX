/**
 * @file DccDrivableCallbacks.cpp
 * @brief Implementation of NmraDcc callback functions for controlling DCC devices.
 *
 * Defines callback functions for handling DCC commands (speed, signals, functions, and accessories)
 * in the MrJ-LayoutFX project. Delegates speed commands to the DccDrivable class and provides
 * placeholders for handling signals, functions, and accessories (e.g., controlling LEDs).
 *
 * @project MrJ-LayoutFX
 * @repo https://github.com/HO44-PROJECT/MrJ-LayoutFX
 * @author MrJ
 * @date 2025-08-21
 * @license MIT License
 */

#include "dcc/DccCallbacks.h"

#ifdef LFX_DCC_ENABLED

/**
 * @brief Handles DCC speed commands for registered devices.
 *
 * Delegates speed commands to the DccDrivable class for devices matching the specified DCC address.
 *
 * @param Addr DCC address of the device (1-10239).
 * @param AddrType Type of address (DCC_ADDR_SHORT or DCC_ADDR_LONG).
 * @param Speed Speed value (0-127 for 128 steps, 0-29 for 28 steps, 0-15 for 14 steps).
 * @param Dir Direction of movement (DCC_DIR_FWD or DCC_DIR_REV).
 * @param SpeedSteps Speed step mode (SPEED_STEP_14, SPEED_STEP_28, SPEED_STEP_128).
 */
__attribute__((used, externally_visible))
void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps)
{
    DccDrivable::notifyDccSpeed(Addr, AddrType, Speed, Dir, SpeedSteps);
}

/**
 * @brief Handles DCC signal output state commands.
 *
 * Delegates signal state commands to the DccDrivable class for devices matching the specified DCC address.
 *
 * @param Addr DCC address of the signal.
 * @param State Signal state (e.g., red or green).
 */
__attribute__((used, externally_visible))
void notifyDccSigOutputState(uint16_t Addr, uint8_t State)
{
    DccDrivable::notifyDccSigOutputState(Addr, State);
}

/**
 * @brief Handles DCC function commands.
 *
 * Provides a placeholder for processing function commands (e.g., controlling LEDs or sounds).
 * Currently logs the command without further action.
 *
 * @param Addr DCC address of the locomotive.
 * @param AddrType Type of address (DCC_ADDR_SHORT or DCC_ADDR_LONG).
 * @param FuncGrp Function group (e.g., FN_0_4, FN_5_8).
 * @param FuncState Function state bitmask (1 = on, 0 = off).
 */
__attribute__((used, externally_visible))
void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState)
{
    DccDrivable::notifyDccFunc(Addr, AddrType, FuncGrp, FuncState);
}

/**
 * @brief Handles DCC accessory commands.
 *
 * Provides a placeholder for processing accessory commands (e.g., turning LEDs on/off).
 * Currently logs the command without further action.
 *
 * @param Addr DCC address of the accessory (1-2044).
 * @param Direction Accessory state (0 = off, 1 = on).
 * @param OutputPower Power level (typically 1 for active).
 */
__attribute__((used, externally_visible))
void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower)
{
    DccDrivable::notifyDccAccTurnoutOutput(Addr, Direction, OutputPower);
}

/**
 * @brief Handles DCC accessory state commands.
 *
 * Delegates accessory state commands to the DccDrivable class for devices matching the specified DCC address.
 *
 * @param Addr DCC address of the accessory (1-2044).
 * @param BoardAddr Board address of the accessory.
 * @param OutputAddr Output address (0-3).
 * @param State Accessory state (0 for off, 1 for on).
 */
__attribute__((used, externally_visible))
void notifyDccAccTurnoutBoard(uint16_t BoardAddr, uint8_t OutputPair, uint8_t Direction, uint8_t OutputPower)
{
    DccDrivable::notifyDccAccTurnoutBoard(BoardAddr, OutputPair, Direction, OutputPower);
}

/**
 * @brief Processes raw DCC messages to handle accessory commands.
 *
 * Analyzes DCC messages to detect accessory packets (first byte between 0x80 and 0xBF),
 * extracts the address and state, and calls notifyDccAccState for further processing.
 *
 * @param Msg Pointer to the DCC message structure containing the packet data.
 */
__attribute__((used, externally_visible))
void notifyDccMsg(DCC_MSG *Msg)
{

    // Check if the packet size is at least 2 bytes to ensure valid data
    if (Msg->Size < 2)
        return;

    // Counts every raw packet, decoded or not — the WebUI diagnostic screen uses this
    // to prove the DCC bus itself is alive even if nothing below matches a known packet type.
    DccDrivable::trackDccMsg(DccDrivable::DCC_MSG_RAW);

#ifdef LFX_DCC_AUDIT_ENABLED
    // Heartbeat: prove the DCC input is alive. If this never prints when a command
    // station is connected, the problem is hardware (pin/opto/wiring), not software.
    {
        static uint16_t _pktCount = 0;
        static unsigned long _lastBeat = 0;
        _pktCount++;
        if (millis() - _lastBeat >= 2000) {
            Serial.print(F("[DCC] "));
            Serial.print(_pktCount);
            Serial.println(F(" packets in last 2s (bus alive)"));
            _pktCount = 0;
            _lastBeat = millis();
        }
    }
#endif

    // Extract the first and second bytes of the DCC packet
    uint8_t b1 = Msg->Data[0]; // First byte
    uint8_t b2 = Msg->Data[1]; // Second byte

    // Filter for Accessory packets (first byte must start with 10xxxxxx, i.e., 0x80 to 0xBF)
    if ((b1 & 0xC0) != 0x80)
        return;

    // Debug: Print raw byte values in hexadecimal (commented out)
    // MRJ_DEBUG_PRINT("Raw: b1=0x"); LOG_PRINT(b1, HEX);
    // MRJ_DEBUG_PRINT(", b2=0x"); LOG_PRINT(b2, HEX);
    // MRJ_DEBUG_PRINTLN();

    // Calculate the board address (1 to 511)
    // Extract the lower 6 bits of the first byte (b1)
    uint8_t low6 = b1 & 0x3F; // Lower 6 bits of b1
    // Extract the upper 3 bits from the second byte (b2), inverted
    uint8_t high3 = (~(b2 >> 4)) & 0x07; // Upper 3 bits, inverted
    // Combine to form the board address (9 bits: high3 << 6 | low6)
    uint16_t boardAddr = ((uint16_t)high3 << 6) | low6;

    // Debug: Print address calculation details (commented out)
    // MRJ_DEBUG_PRINT("Address calc: low6=0x"); LOG_PRINT(low6, HEX);
    // MRJ_DEBUG_PRINT(", high3=0x"); LOG_PRINT(high3, HEX);
    // MRJ_DEBUG_PRINT(", BoardAddr="); LOG_PRINTLN(boardAddr);

    // Check if the packet is a Basic Accessory packet (b2 bit 7 = 1) or Extended Accessory packet (b2 bit 7 = 0)
    if (b2 & 0x80)
    {
        // === BASIC ACCESSORY PACKET ===
        // Extract state (ON=1, OFF=0) from bit 3 of the second byte
        uint8_t state = (b2 & 0x08) ? 1 : 0; // State (ON=1, OFF=0)
        // Extract the 3 DDD bits (bits 0-2 of b2)
        uint8_t ddd = b2 & 0x07; // 3 DDD bits
        // Calculate pair (0 or 1) by shifting DDD right by 1
        uint8_t pair = ddd >> 1; // Pair (0 to 1)
        // Extract direction (0 or 1) from the least significant bit of DDD
        uint8_t dir = ddd & 0x01; // Direction (0 or 1)

        // Calculate the accessory address: ((boardAddr - 1) * 4) + pair + 1
        uint16_t addr = (((boardAddr - 1) << 2) | pair) + 1;

#ifdef LFX_DCC_AUDIT_ENABLED
        Serial.print(F("[DCC] accessory addr "));
        Serial.print(addr);
        Serial.print(F(" state "));
        Serial.println(state);
#endif

        // Debug: Print Basic Accessory packet details (commented out)
        // MRJ_DEBUG_PRINT("Basic Packet: BoardAddr="); LOG_PRINT(boardAddr);
        // MRJ_DEBUG_PRINT(", Pair="); LOG_PRINT(pair);
        // MRJ_DEBUG_PRINT(", Dir="); LOG_PRINT(dir);
        // MRJ_DEBUG_PRINT(", State="); LOG_PRINT(state);
        // MRJ_DEBUG_PRINT(", CalcAddr="); LOG_PRINTLN(addr);

        // Check if in output-oriented mode
        if (DccDrivable::IS_OUTPUT_MODE())
        {
            // Output-oriented mode: One output per address
            notifyDccAccTurnoutOutput(addr, dir, state);
        }
        else
        {
            // Board-oriented mode: Four outputs per board
            notifyDccAccTurnoutBoard(addr, pair, dir, state);
        }
    }
    else
    {
        // === EXTENDED ACCESSORY PACKET (Signal) ===
        // Ensure packet size is at least 3 bytes for signal packets
        if (Msg->Size < 3)
            return; // Check size for signal packets
        // Extract the aspect (6 bits) from the third byte
        uint8_t aspect = Msg->Data[2] & 0x3F; // 6 bits for aspect
        // Extract the 3 DDD bits (bits 0-2 of b2)
        uint8_t ddd = b2 & 0x07; // 3 DDD bits
        // Calculate pair (0 or 1) by shifting DDD right by 1
        uint8_t pair = ddd >> 1; // Pair (0 to 1)
        // Calculate the accessory address: ((boardAddr - 1) * 4) + pair + 1
        uint16_t addr = (((boardAddr - 1) << 2) | pair) + 1;

#ifdef LFX_DCC_AUDIT_ENABLED
        Serial.print(F("[DCC] signal addr "));
        Serial.print(addr);
        Serial.print(F(" aspect "));
        Serial.println(aspect);
#endif

        // Debug: Print Extended Accessory packet details (commented out)
        // MRJ_DEBUG_PRINT("Extended Packet: BoardAddr="); LOG_PRINT(boardAddr);
        // MRJ_DEBUG_PRINT(", Pair="); LOG_PRINT(pair);
        // MRJ_DEBUG_PRINT(", Aspect="); LOG_PRINT(aspect);
        // MRJ_DEBUG_PRINT(", CalcAddr="); LOG_PRINTLN(addr);

        // Notify signal output state with the calculated address and aspect
        notifyDccSigOutputState(addr, aspect);
    }
}

#endif

void forceLinkDccCallbacks() {}

