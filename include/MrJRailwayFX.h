/**
 * @file MrJRailwayFX.h
 * @brief Main include file for the MrJ-RailwayFX library.
 *
 * This header aggregates all public includes for the MrJ-RailwayFX library,
 * providing a single entry point for consuming projects.
 *
 * @project MrJ-ArduinoRailwayFX
 * @repo https://github.com/HO44-PROJECT/MrJ-ArduinoRailwayFX
 * @author MrJ
 * @date 2025-08-04
 * @license MIT License. See the LICENSE file in the project root for details.
 */

#ifndef __MRJ_RAILWAY_FX_H__
#define __MRJ_RAILWAY_FX_H__

#include <core/Core.h>

#include <audio/DfAudio.h>

#include "dcc/DccCallbacks.h"
#include "dcc/DccDrivable.h"

#include "devices/StaticLow.h"
#include "devices/StaticOpen.h"
#include "devices/StaticUp.h"

#include "led_fx/Beacon.h"
#include "led_fx/CampFire.h"
#include "led_fx/DefectLamp.h"
#include "led_fx/DoubleBeacon.h"
#include "led_fx/ElectricLamp.h"
#include "led_fx/GasLamp.h"
#include "led_fx/NeonSign.h"
#include "led_fx/OilLamp.h"
#include "led_fx/RailwayCrossingLights.h"
#include "led_fx/SignalFlare.h"
#include "led_fx/SolderLamp.h"
#include "led_fx/Storm.h"
#include "led_fx/Torch.h"
#include "led_fx/TrainHeadLamp.h"
#include "led_fx/TurnSignal.h"

#include "oled/StatusOled.h"

#include "servo/SerialServoMotorMode.h"

#include "signals/MrJDbBlocSignal.h"
#include "signals/MrJDbEntrySignal.h"
#include "signals/MrJDbExitSignal.h"

#include "traffic/TrafficLight3Phase.h"
#include "traffic/TrafficLight4Phase.h"

#include "utils/utils.h"

#include "api/ApiServer.h"
#include "config/ConfigManager.h"

#endif // __MRJ_RAILWAY_FX_H__
