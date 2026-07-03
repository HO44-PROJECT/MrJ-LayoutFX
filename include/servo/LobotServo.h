/**
 * @file LobotServo.h
 * @brief Lightweight, non-blocking servo control class for LX-16A-like servos on Arduino Nano.
 *
 * This class is an optimized alternative to the lx16a-servo library, designed for the Arduino Nano
 * (ATmega328P) with limited resources (2 KB SRAM, 32 KB flash). It implements the full logic from
 * SerialServo.h, with a focus on minimal memory usage and non-blocking operation for AceRoutine
 * coroutines. A single shared buffer is used for all commands, and read operations use a state
 * machine to avoid blocking. The class is compatible with the motor_mode signature from LX16AServo.
 *
 * Key features:
 * - Single class-level buffer (_cmdBuf[10]) for all commands, reducing SRAM usage.
 * - Non-blocking read operations using coroutine-friendly state machine.
 * - Implements all SerialServo.h functions (move, readPosition, setID, etc.).
 * - Uses Lobot protocol for command framing and checksum, adapted from SerialServo.h.
 * - Optimized for Nano's single serial port (pins 0/1) and limited resources.
 * - No retry or debug logic to minimize memory usage.
 * - Assumes Serial is used; configure externally for single-pin or direction pin setups.
 *
 * @note User must call Serial.begin(115200) before instantiation.
 * @note For single-pin mode, configure TX/RX pin (e.g., pin 1) externally.
 * @note Read operations require calling runCoroutine() to process responses.
 * @note Error codes: -2048 (timeout), -2049 (receive error).
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author Grok 4 (based on user specifications)
 * @date 2025-09-23
 * @license MIT License.
 */

#pragma once

#include <LayoutFX_define.h>

#ifdef LFX_LOBOT_SERVO_ENABLED

#include <Arduino.h>
#include <AceRoutine.h> // For coroutine support

// Macros from include.h (included for self-containment)
#define GET_LOW_BYTE(A) (uint8_t)((A))  // Get low 8 bits of A
#define GET_HIGH_BYTE(A) (uint8_t)((A) >> 8)  // Get high 8 bits of A
#define BYTE_TO_HW(A, B) ((((uint16_t)(A)) << 8) | (uint8_t)(B))  // Combine A (high) and B (low) into 16-bit value

#define ID_ALL 254  // Broadcast ID for all servos

#define LOBOT_SERVO_FRAME_HEADER         0x55
#define LOBOT_SERVO_MOVE_TIME_WRITE      1
#define LOBOT_SERVO_MOVE_TIME_READ       2
#define LOBOT_SERVO_MOVE_TIME_WAIT_WRITE 7
#define LOBOT_SERVO_MOVE_TIME_WAIT_READ  8
#define LOBOT_SERVO_MOVE_START           11
#define LOBOT_SERVO_MOVE_STOP            12
#define LOBOT_SERVO_ID_WRITE             13
#define LOBOT_SERVO_ID_READ              14
#define LOBOT_SERVO_ANGLE_OFFSET_ADJUST  17
#define LOBOT_SERVO_ANGLE_OFFSET_WRITE   18
#define LOBOT_SERVO_ANGLE_OFFSET_READ    19
#define LOBOT_SERVO_ANGLE_LIMIT_WRITE    20
#define LOBOT_SERVO_ANGLE_LIMIT_READ     21
#define LOBOT_SERVO_VIN_LIMIT_WRITE      22
#define LOBOT_SERVO_VIN_LIMIT_READ       23
#define LOBOT_SERVO_TEMP_MAX_LIMIT_WRITE 24
#define LOBOT_SERVO_TEMP_MAX_LIMIT_READ  25
#define LOBOT_SERVO_TEMP_READ            26
#define LOBOT_SERVO_VIN_READ             27
#define LOBOT_SERVO_POS_READ             28
#define LOBOT_SERVO_OR_MOTOR_MODE_WRITE  29
#define LOBOT_SERVO_OR_MOTOR_MODE_READ   30
#define LOBOT_SERVO_LOAD_OR_UNLOAD_WRITE 31
#define LOBOT_SERVO_LOAD_OR_UNLOAD_READ  32
#define LOBOT_SERVO_LED_CTRL_WRITE       33
#define LOBOT_SERVO_LED_CTRL_READ        34
#define LOBOT_SERVO_LED_ERROR_WRITE      35
#define LOBOT_SERVO_LED_ERROR_READ       36

/**
 * @class LobotServo
 * @brief Lightweight, non-blocking servo control for LX-16A-like servos on Arduino Nano.
 *
 * Provides a drop-in replacement for LX16AServo, with all SerialServo.h functions implemented.
 * Uses a single shared buffer for commands and a state machine for non-blocking reads, optimized
 * for AceRoutine coroutines. Designed for Nano's single serial port (pins 0/1).
 *
 * @note Call runCoroutine() in the main loop to process read responses.
 * @note Range read results (e.g., angle limits) are stored in members and accessed via getters.
 */
class LobotServo : public ace_routine::Coroutine {
private:
    HardwareSerial &_bus;  ///< Reference to HardwareSerial (e.g., Serial on Nano).
    uint8_t _id;           ///< Servo ID (0-253, or 254 for broadcast).
    byte _cmdBuf[10];      ///< Shared buffer for all commands (max size 10 from move/setMode).

    // Stored values from range reads
    int16_t _angleLow = 0;   ///< Low angle limit from last readAngleRange
    int16_t _angleHigh = 0;  ///< High angle limit from last readAngleRange
    int16_t _vinLow = 0;     ///< Low vin limit from last readVinLimit
    int16_t _vinHigh = 0;    ///< High vin limit from last readVinLimit

    // State machine for non-blocking receive
    enum ReceiveState {
        IDLE,
        WAIT_HEADER1,
        WAIT_HEADER2,
        WAIT_ID,
        WAIT_LENGTH,
        WAIT_DATA,
        WAIT_CHECKSUM
    };
    ReceiveState _receiveState = IDLE;  ///< Current state of receive state machine
    byte _recvBuf[10];                 ///< Buffer for received data
    byte _dataCount = 0;               ///< Current byte count in receive
    byte _dataLength = 2;              ///< Expected data length
    byte _frameCount = 0;              ///< Header frame counter
    uint32_t _timeoutStart = 0;        ///< Timeout start time (ms)
    int _lastReadResult = -2048;       ///< Last read result (-2048 = not ready)
    uint8_t _lastReadCmd = 0;          ///< Last command sent for read operation
    uint8_t _healthMode  = 0;          ///< Mode read by last healthCheckBlocking (0=pos, 1=motor)
    int16_t _healthSpeed = 0;          ///< Speed read by last healthCheckBlocking

    /**
     * @brief Computes the checksum for a command buffer.
     *
     * Adapted from LobotCheckSum in SerialServo.h. Sums bytes from index 2 to (buf[3] + 1),
     * then returns the bitwise NOT of the sum.
     *
     * @param buf The command buffer.
     * @return The computed checksum byte.
     */
    byte checkSum(byte buf[]) {
        uint16_t temp = 0;
        for (byte i = 2; i < buf[3] + 2; i++) {
            temp += buf[i];
        }
        temp = ~temp;
        return (byte)temp;
    }

    /**
     * @brief Initializes a read operation by sending the command and setting up the state machine.
     *
     * Clears the serial buffer, sends the command, and sets the state to WAIT_HEADER1.
     *
     * @param cmd The command to send (e.g., LOBOT_SERVO_POS_READ).
     */
    void startRead(uint8_t cmd) {
        _cmdBuf[0] = _cmdBuf[1] = LOBOT_SERVO_FRAME_HEADER;
        _cmdBuf[2] = _id;
        _cmdBuf[3] = 3;
        _cmdBuf[4] = cmd;
        _cmdBuf[5] = checkSum(_cmdBuf);
        _bus.write(_cmdBuf, 6);
        _bus.flush();

        while (_bus.available()) _bus.read(); // Clear buffer
        _receiveState = WAIT_HEADER1;
        _dataCount = 0;
        _frameCount = 0;
        _dataLength = 2;
        _timeoutStart = millis();
        _lastReadResult = -2048; // Not ready
        _lastReadCmd = cmd;
    }

public:
    /**
     * @brief Constructs a LobotServo instance.
     *
     * Initializes the servo with the provided serial bus and ID. The serial port must be
     * pre-configured with Serial.begin(115200). For single-pin mode, ensure the TX/RX pin
     * (e.g., pin 1) is set up externally.
     *
     * @param bus Reference to HardwareSerial (e.g., Serial).
     * @param id Servo ID (0-253, or 254 for broadcast).
     */
    LobotServo(HardwareSerial &bus, uint8_t id) : _bus(bus), _id(id) {
        _bus.setTimeout(5); // 5 ms — enough for echo at 115200 baud (10 bytes ≈ 0.87 ms)
    }

    /**
     * @brief Coroutine to process non-blocking read responses.
     *
     * Runs the state machine to handle incoming data packets. Must be called in the main loop.
     * Updates _lastReadResult and range variables (e.g., _angleLow) on completion.
     *
     * @return 0 on success (AceRoutine convention).
     */
    int runCoroutine() override {
        COROUTINE_LOOP() {
            if (_receiveState == IDLE) {
                COROUTINE_YIELD(); // Nothing to process
            }

            if (!_bus.available()) {
                if (millis() - _timeoutStart > 100) { // 100ms timeout
                    _receiveState = IDLE;
                    _lastReadResult = -2048;
                    COROUTINE_YIELD();
                }
                COROUTINE_YIELD();
            }

            byte rxBuf = _bus.read();
            switch (_receiveState) {
                case WAIT_HEADER1:
                    if (rxBuf == LOBOT_SERVO_FRAME_HEADER) {
                        _frameCount++;
                        if (_frameCount == 1) {
                            _receiveState = WAIT_HEADER2;
                        }
                    } else {
                        _frameCount = 0;
                    }
                    break;
                case WAIT_HEADER2:
                    if (rxBuf == LOBOT_SERVO_FRAME_HEADER) {
                        _frameCount++;
                        if (_frameCount == 2) {
                            _frameCount = 0;
                            _receiveState = WAIT_ID;
                            _dataCount = 1;
                            _recvBuf[1] = rxBuf;
                        }
                    } else {
                        _frameCount = 0;
                        _receiveState = WAIT_HEADER1;
                    }
                    break;
                case WAIT_ID:
                    _recvBuf[_dataCount++] = rxBuf;
                    _receiveState = WAIT_LENGTH;
                    break;
                case WAIT_LENGTH:
                    _recvBuf[_dataCount++] = rxBuf;
                    _dataLength = rxBuf;
                    if (_dataLength < 3 || _dataLength > 7) {
                        _receiveState = IDLE;
                        _lastReadResult = -2048;
                    } else {
                        _receiveState = WAIT_DATA;
                    }
                    break;
                case WAIT_DATA:
                    _recvBuf[_dataCount++] = rxBuf;
                    if (_dataCount == _dataLength + 2) {
                        _receiveState = WAIT_CHECKSUM;
                    }
                    break;
                case WAIT_CHECKSUM:
                    _recvBuf[_dataCount] = rxBuf;
                    if (checkSum(_recvBuf) == rxBuf) {
                        // Process response based on last command
                        switch (_lastReadCmd) {
                            case LOBOT_SERVO_ID_READ:
                                _lastReadResult = (int16_t)BYTE_TO_HW(0x00, _recvBuf[4]);
                                break;
                            case LOBOT_SERVO_POS_READ:
                            case LOBOT_SERVO_ANGLE_OFFSET_READ:
                                _lastReadResult = (int16_t)BYTE_TO_HW(_recvBuf[5], _recvBuf[4]);
                                break;
                            case LOBOT_SERVO_ANGLE_LIMIT_READ:
                                _angleLow = (int16_t)BYTE_TO_HW(_recvBuf[5], _recvBuf[4]);
                                _angleHigh = (int16_t)BYTE_TO_HW(_recvBuf[7], _recvBuf[6]);
                                _lastReadResult = 0;
                                break;
                            case LOBOT_SERVO_VIN_READ:
                                _lastReadResult = (int16_t)BYTE_TO_HW(_recvBuf[5], _recvBuf[4]);
                                break;
                            case LOBOT_SERVO_VIN_LIMIT_READ:
                                _vinLow = (int16_t)BYTE_TO_HW(_recvBuf[5], _recvBuf[4]);
                                _vinHigh = (int16_t)BYTE_TO_HW(_recvBuf[7], _recvBuf[6]);
                                _lastReadResult = 0;
                                break;
                            case LOBOT_SERVO_TEMP_MAX_LIMIT_READ:
                            case LOBOT_SERVO_TEMP_READ:
                            case LOBOT_SERVO_LOAD_OR_UNLOAD_READ:
                                _lastReadResult = (int16_t)BYTE_TO_HW(0x00, _recvBuf[4]);
                                break;
                            default:
                                _lastReadResult = -2049;
                                break;
                        }
                    } else {
                        _lastReadResult = -2049; // Checksum error
                    }
                    _receiveState = IDLE;
                    break;
            }
            COROUTINE_YIELD();
        }
        return 0;
    }

    /**
     * @brief Sets the servo to motor mode or position mode with a given speed.
     *
     * Compatible with LX16AServo::motor_mode. Sets position mode (mode 0) if speed == 0,
     * or motor mode (mode 1) with the specified speed (-1000 to 1000) otherwise.
     *
     * @param speed Target speed (-1000 for full reverse, 0 for stop/position mode, 1000 for full forward).
     */
    void motor_mode(int16_t speed) {
        uint8_t mode = (speed != 0) ? 1 : 0;
        _cmdBuf[0] = _cmdBuf[1] = LOBOT_SERVO_FRAME_HEADER;
        _cmdBuf[2] = _id;
        _cmdBuf[3] = 7;
        _cmdBuf[4] = LOBOT_SERVO_OR_MOTOR_MODE_WRITE;
        _cmdBuf[5] = mode;
        _cmdBuf[6] = 0;
        _cmdBuf[7] = GET_LOW_BYTE((uint16_t)speed);
        _cmdBuf[8] = GET_HIGH_BYTE((uint16_t)speed);
        _cmdBuf[9] = checkSum(_cmdBuf);
        _bus.write(_cmdBuf, 10);
        _bus.flush(); // wait for TX complete before reading echo
        // Discard the 10-byte half-duplex echo (TX mirrored on RX)
        uint8_t echo[10];
        _bus.readBytes(echo, 10);
    }

    /**
     * @brief Writes a new ID to the servo.
     *
     * @param oldID Current ID (or ID_ALL for broadcast).
     * @param newID New ID to set (0-253).
     */
    void setID(uint8_t oldID, uint8_t newID) {
        _cmdBuf[0] = _cmdBuf[1] = LOBOT_SERVO_FRAME_HEADER;
        _cmdBuf[2] = oldID;
        _cmdBuf[3] = 4;
        _cmdBuf[4] = LOBOT_SERVO_ID_WRITE;
        _cmdBuf[5] = newID;
        _cmdBuf[6] = checkSum(_cmdBuf);
        _bus.write(_cmdBuf, 7);
        _bus.flush();
    }

    /**
     * @brief Moves the servo to a position over a specified time.
     *
     * @param position Target position (0-1000).
     * @param time Movement time in ms.
     */
    void move(int16_t position, uint16_t time) {
        if (position < 0) position = 0;
        if (position > 1000) position = 1000;
        _cmdBuf[0] = _cmdBuf[1] = LOBOT_SERVO_FRAME_HEADER;
        _cmdBuf[2] = _id;
        _cmdBuf[3] = 7;
        _cmdBuf[4] = LOBOT_SERVO_MOVE_TIME_WRITE;
        _cmdBuf[5] = GET_LOW_BYTE(position);
        _cmdBuf[6] = GET_HIGH_BYTE(position);
        _cmdBuf[7] = GET_LOW_BYTE(time);
        _cmdBuf[8] = GET_HIGH_BYTE(time);
        _cmdBuf[9] = checkSum(_cmdBuf);
        _bus.write(_cmdBuf, 10);
        _bus.flush();
    }

    /**
     * @brief Confirms the servo is alive by reading its motor mode (blocking).
     *
     * Sends LOBOT_SERVO_OR_MOTOR_MODE_READ (cmd 30), discards the half-duplex echo,
     * then reads and validates the 10-byte response. Called from healthCheck();
     * safe to call while the control coroutine is parked.
     *
     * @param timeoutMs Maximum wait time per phase (default 20 ms).
     * @return 0 = OK, 1 = no response (timeout), 2 = bad response (frame/checksum).
     */
    int healthCheckBlocking(uint16_t timeoutMs = 20) {
        _receiveState = IDLE; // ensure async SM is not mid-frame

        // Drain any leftover bytes in RX (e.g. undiscarded echo from motor_mode).
        while (_bus.available()) _bus.read();

        // Build and send the query (6 bytes)
        _cmdBuf[0] = _cmdBuf[1] = LOBOT_SERVO_FRAME_HEADER;
        _cmdBuf[2] = _id;
        _cmdBuf[3] = 3;
        _cmdBuf[4] = LOBOT_SERVO_OR_MOTOR_MODE_READ;
        _cmdBuf[5] = checkSum(_cmdBuf);
        _bus.write(_cmdBuf, 6);
        _bus.flush();

        // Discard 6-byte half-duplex echo
        uint32_t t = millis();
        uint8_t n = 0;
        while (n < 6 && (millis() - t) < timeoutMs) {
            if (_bus.available()) { _bus.read(); n++; }
        }
        if (n < 6) return 1;

        // Read 10-byte response: 0x55 0x55 id 0x07 cmd mode 0x00 speedL speedH chk
        uint8_t resp[10];
        n = 0;
        t = millis();
        while (n < 10 && (millis() - t) < timeoutMs) {
            if (_bus.available()) resp[n++] = _bus.read();
        }
        if (n < 10) return 1;

        // Validate frame header and checksum
        if (resp[0] != LOBOT_SERVO_FRAME_HEADER || resp[1] != LOBOT_SERVO_FRAME_HEADER)
            return 2;
        if (checkSum(resp) != resp[9]) return 2;

        // resp layout: 0x55 0x55 id len cmd mode 0x00 speedL speedH chk
        _healthMode  = resp[5];
        _healthSpeed = (int16_t)((uint16_t)resp[7] | ((uint16_t)resp[8] << 8));
        return 0;
    }

    uint8_t getHealthMode()  const { return _healthMode;  }
    int16_t getHealthSpeed() const { return _healthSpeed; }

    /**
     * @brief Initiates reading the servo ID.
     *
     * Starts a non-blocking read; call runCoroutine() to process the response.
     * @return Current result (-2048 = not ready, ID or error code when done).
     */
    int readID() {
        if (_receiveState == IDLE) {
            startRead(LOBOT_SERVO_ID_READ);
        }
        return _lastReadResult;
    }

    /**
     * @brief Initiates reading the current servo position.
     *
     * Starts a non-blocking read; call runCoroutine() to process the response.
     * @return Current result (-2048 = not ready, position or error code when done).
     */
    int readPosition() {
        if (_receiveState == IDLE) {
            startRead(LOBOT_SERVO_POS_READ);
        }
        return _lastReadResult;
    }

    /**
     * @brief Initiates reading the angle offset.
     *
     * Starts a non-blocking read; call runCoroutine() to process the response.
     * @return Current result (-2048 = not ready, offset or error code when done).
     */
    int readOffset() {
        if (_receiveState == IDLE) {
            startRead(LOBOT_SERVO_ANGLE_OFFSET_READ);
        }
        return _lastReadResult;
    }

    /**
     * @brief Initiates reading the angle range limits.
     *
     * Starts a non-blocking read; call runCoroutine() to process. Use getAngleLow() and getAngleHigh().
     * @return Current result (-2048 = not ready, 0 or error code when done).
     */
    int readAngleRange() {
        if (_receiveState == IDLE) {
            startRead(LOBOT_SERVO_ANGLE_LIMIT_READ);
        }
        return _lastReadResult;
    }

    /**
     * @brief Gets the low angle limit from the last readAngleRange call.
     * @return Low angle limit.
     */
    int16_t getAngleLow() const { return _angleLow; }

    /**
     * @brief Gets the high angle limit from the last readAngleRange call.
     * @return High angle limit.
     */
    int16_t getAngleHigh() const { return _angleHigh; }

    /**
     * @brief Initiates reading the input voltage.
     *
     * Starts a non-blocking read; call runCoroutine() to process the response.
     * @return Current result (-2048 = not ready, voltage or error code when done).
     */
    int readVin() {
        if (_receiveState == IDLE) {
            startRead(LOBOT_SERVO_VIN_READ);
        }
        return _lastReadResult;
    }

    /**
     * @brief Initiates reading the voltage limits.
     *
     * Starts a non-blocking read; call runCoroutine() to process. Use getVinLow() and getVinHigh().
     * @return Current result (-2048 = not ready, 0 or error code when done).
     */
    int readVinLimit() {
        if (_receiveState == IDLE) {
            startRead(LOBOT_SERVO_VIN_LIMIT_READ);
        }
        return _lastReadResult;
    }

    /**
     * @brief Gets the low vin limit from the last readVinLimit call.
     * @return Low vin limit.
     */
    int16_t getVinLow() const { return _vinLow; }

    /**
     * @brief Gets the high vin limit from the last readVinLimit call.
     * @return High vin limit.
     */
    int16_t getVinHigh() const { return _vinHigh; }

    /**
     * @brief Initiates reading the temperature alarm threshold.
     *
     * Starts a non-blocking read; call runCoroutine() to process the response.
     * @return Current result (-2048 = not ready, threshold or error code when done).
     */
    int readTempLimit() {
        if (_receiveState == IDLE) {
            startRead(LOBOT_SERVO_TEMP_MAX_LIMIT_READ);
        }
        return _lastReadResult;
    }

    /**
     * @brief Initiates reading the current temperature.
     *
     * Starts a non-blocking read; call runCoroutine() to process the response.
     * @return Current result (-2048 = not ready, temperature or error code when done).
     */
    int readTemp() {
        if (_receiveState == IDLE) {
            startRead(LOBOT_SERVO_TEMP_READ);
        }
        return _lastReadResult;
    }

    /**
     * @brief Initiates reading the load/unload status.
     *
     * Starts a non-blocking read; call runCoroutine() to process the response.
     * @return Current result (-2048 = not ready, 0/1 or error code when done).
     */
    int readLoadOrUnload() {
        if (_receiveState == IDLE) {
            startRead(LOBOT_SERVO_LOAD_OR_UNLOAD_READ);
        }
        return _lastReadResult;
    }

    /**
     * @brief Stops the servo movement.
     */
    void stopMove() {
        _cmdBuf[0] = _cmdBuf[1] = LOBOT_SERVO_FRAME_HEADER;
        _cmdBuf[2] = _id;
        _cmdBuf[3] = 3;
        _cmdBuf[4] = LOBOT_SERVO_MOVE_STOP;
        _cmdBuf[5] = checkSum(_cmdBuf);
        _bus.write(_cmdBuf, 6);
        _bus.flush();
    }

    /**
     * @brief Sets the servo mode and speed.
     *
     * @param mode 0 (position) or 1 (motor).
     * @param speed Speed for motor mode (-1000 to 1000).
     */
    void setMode(uint8_t mode, int16_t speed) {
        _cmdBuf[0] = _cmdBuf[1] = LOBOT_SERVO_FRAME_HEADER;
        _cmdBuf[2] = _id;
        _cmdBuf[3] = 7;
        _cmdBuf[4] = LOBOT_SERVO_OR_MOTOR_MODE_WRITE;
        _cmdBuf[5] = mode;
        _cmdBuf[6] = 0;
        _cmdBuf[7] = GET_LOW_BYTE((uint16_t)speed);
        _cmdBuf[8] = GET_HIGH_BYTE((uint16_t)speed);
        _cmdBuf[9] = checkSum(_cmdBuf);
        _bus.write(_cmdBuf, 10);
        _bus.flush();
    }

    /**
     * @brief Loads (enables) the servo motor.
     */
    void load() {
        _cmdBuf[0] = _cmdBuf[1] = LOBOT_SERVO_FRAME_HEADER;
        _cmdBuf[2] = _id;
        _cmdBuf[3] = 4;
        _cmdBuf[4] = LOBOT_SERVO_LOAD_OR_UNLOAD_WRITE;
        _cmdBuf[5] = 1;
        _cmdBuf[6] = checkSum(_cmdBuf);
        _bus.write(_cmdBuf, 7);
        _bus.flush();
    }

    /**
     * @brief Unloads (disables) the servo motor.
     */
    void unload() {
        _cmdBuf[0] = _cmdBuf[1] = LOBOT_SERVO_FRAME_HEADER;
        _cmdBuf[2] = _id;
        _cmdBuf[3] = 4;
        _cmdBuf[4] = LOBOT_SERVO_LOAD_OR_UNLOAD_WRITE;
        _cmdBuf[5] = 0;
        _cmdBuf[6] = checkSum(_cmdBuf);
        _bus.write(_cmdBuf, 7);
        _bus.flush();
    }
};

#endif // LFX_LOBOT_SERVO_ENABLED
