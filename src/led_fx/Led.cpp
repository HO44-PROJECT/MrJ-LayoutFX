/**
 * @file Led.cpp
 * @brief Implements the `Led` class for a basic LED on/off effect.
 */

#include "led_fx/Led.h"

int Led::runCoroutine()
{
    COROUTINE_LOOP()
    {
        DEVICE_WAIT_STATE_CHANGE(getTargetState());
        DEVICE_APPLY_START_DELAY();

        if (getTargetState() == ON_STATE)
        {
            if (getState() == INIT_STATE && !handlePinInitFailure())
                continue;
            outputActive(_pin);
            setState(RUN_STABLE_STATE);
        }
        else
        {
            outputInactive(_pin);
            setState(OFF_STATE);
        }
    }
    return 0;
}
