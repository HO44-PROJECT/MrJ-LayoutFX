#pragma once
#include "IModule.h"
#include <Arduino.h>

class Core {
public:
    void init();
    void setup();
    void loop();

private:
    static constexpr size_t MAX_MODULES = 8;

    IModule* modules[MAX_MODULES];
    size_t moduleCount = 0;

    void registerModule(IModule* module);
};