/**
 * @file SerialServoMotorMode.h
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

#ifndef __MRJRAILWAYFX_CONFIGURE_H__
#define __MRJRAILWAYFX_CONFIGURE_H__

#include "MrJRailwayFX_default.h"

#ifdef CONFIGURE
#include CONFIGURE
#endif

#endif // __MRJRAILWAYFX_CONFIGURE_H__