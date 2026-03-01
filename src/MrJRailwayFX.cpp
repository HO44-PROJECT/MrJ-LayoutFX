#include <core/Core.h>

void Core::registerModule(IModule* module) {
    if (moduleCount < MAX_MODULES) {
        modules[moduleCount++] = module;
    }
}

void Core::init() {

#ifdef FEATURE_LED
    registerModule(&led);
#endif

#ifdef FEATURE_DCC
    registerModule(&dcc);
#endif

#ifdef FEATURE_WIFI
    registerModule(&wifi);
#endif
}

void Core::setup() {
    for (size_t i = 0; i < moduleCount; i++) {
        modules[i]->setup();
    }
}

void Core::loop() {
    for (size_t i = 0; i < moduleCount; i++) {
        modules[i]->loop();
    }
}