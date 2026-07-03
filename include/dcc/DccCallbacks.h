#pragma once

#include <LayoutFX_define.h>

#ifdef LFX_DCC_ENABLED

#include <Arduino.h>
#include <NmraDcc.h>
#include "dcc/DccDrivable.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType,
                        uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps);

    void notifyDccSigOutputState(uint16_t Addr, uint8_t State);

    void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType,
                       FN_GROUP FuncGrp, uint8_t FuncState);

    void notifyDccAccTurnoutOutput(uint16_t Addr, uint8_t Direction, uint8_t OutputPower);

    void notifyDccAccTurnoutBoard(uint16_t BoardAddr, uint8_t OutputPair,
                                  uint8_t Direction, uint8_t OutputPower);

    void notifyDccMsg(DCC_MSG *Msg);

#ifdef __cplusplus
}
#endif

#endif  // LFX_DCC_ENABLED
