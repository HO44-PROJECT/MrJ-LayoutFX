#pragma once

class IModule {
public:
    virtual ~IModule() = default;

    virtual void setup() = 0;
    virtual void loop() = 0;
};